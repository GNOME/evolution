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
#include <bonobo-conf/bonobo-config-database.h>

#include "e-summary.h"
#include "e-summary-preferences.h"
#include "e-summary-table.h"

#include "evolution-config-control.h"


#define FACTORY_ID "OAFIID:GNOME_Evolution_Summary_ConfigControlFactory"


static void
make_initial_mail_list (ESummaryPrefs *prefs)
{
	char *evolution_dir;
	GList *folders;

	evolution_dir = gnome_util_prepend_user_home ("evolution/local");

	folders = g_list_append (NULL, g_strconcat (evolution_dir, "/Inbox", NULL));
	folders = g_list_append (folders, g_strconcat (evolution_dir, "/Outbox", NULL));

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
	vector = bonobo_config_get_string (db, "My-Evolution/Mail/display_folders", &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Error getting Mail/display_folders");
		CORBA_exception_free (&ev);
		bonobo_object_release_unref (db, NULL);
		return FALSE;
	}
	prefs->display_folders = str_list_from_vector (vector);
  	g_free (vector);

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

	vector = vector_from_str_list (prefs->display_folders);
	bonobo_config_set_string (db, "My-Evolution/Mail/display_folders", vector, NULL);
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

void
e_summary_preferences_free (ESummaryPrefs *prefs)
{
	if (prefs->display_folders) {
		free_str_list (prefs->display_folders);
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

ESummaryPrefs *
e_summary_preferences_copy (ESummaryPrefs *prefs)
{
	ESummaryPrefs *prefs_copy;

	prefs_copy = g_new (ESummaryPrefs, 1);

	prefs_copy->display_folders = copy_str_list (prefs->display_folders);
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

void
e_summary_preferences_init (ESummary *summary)
{
	ESummaryPrefs *prefs;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	summary->preferences = g_new0 (ESummaryPrefs, 1);
	summary->old_prefs = NULL;

	if (e_summary_preferences_restore (summary->preferences) == TRUE) {
		return;
	}

	prefs = summary->preferences;
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
}

struct _MailPage {
	GtkWidget *etable;
	GtkWidget *all, *shown;
	GtkWidget *fullpath;
	GtkWidget *add, *remove;

	GHashTable *model;
	GList *tmp_list;
};

struct _RDFPage {
	GtkWidget *etable;
	GtkWidget *refresh, *limit;
	GtkWidget *new_url, *delete_url;

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

	ESummary *summary;
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
	{"http://www.cnn.com/cnn.rss", "CNN", FALSE},
        {"http://www.debianplanet.org/debianplanet/backend.php", "Debian Planet", FALSE},
	{"http://www.dictionary.com/wordoftheday/wotd.rss", N_("Dictionary.com Word of the Day"), FALSE},
	{"http://www.dvdreview.com/rss/newschannel.rss", "DVD Review", FALSE},
	{"http://freshmeat.net/backend/fm.rdf", "Freshmeat", FALSE},
	{"http://news.gnome.org/gnome-news/rdf", "GNotices", FALSE},
	{"http://headlines.internet.com/internetnews/prod-news/news.rss", "Internet.com", FALSE},
	{"http://www.hispalinux.es/backend.php", "HispaLinux", FALSE},
	{"http://dot.kde.org/rdf", "KDE Dot News", FALSE},
	{"http://www.kuro5hin.org/backend.rdf", "Kuro5hin", FALSE},
	{"http://linuxgames.com/bin/mynetscape.pl", "Linux Games", FALSE},
	{"http://linux.com/mrn/jobs/latest_jobs.rss", "Linux Jobs", FALSE},
	{"http://linuxtoday.com/backend/my-netscape.rdf", "Linux Today", FALSE},
	{"http://lwn.net/headlines/rss", "Linux Weekly News", FALSE},
	{"http://www.linux.com/mrn/front_page.rss", "Linux.com", FALSE},
	{"http://memepool.com/memepool.rss", "Memepool", FALSE},
	{"http://www.mozilla.org/news.rdf", "Mozilla", FALSE},
	{"http://www.mozillazine.org/contents.rdf", "Mozillazine", FALSE},
	{"http://www.fool.com/about/headlines/rss_headlines.asp", "The Motley Fool", FALSE},
	{"http://www.newsforge.com/newsforge.rss", "Newsforge", FALSE},
	{"http://www.nanotechnews.com/nano/rdf", "Nanotech News", FALSE},
	{"http://www.pigdog.org/pigdog.rdf", "Pigdog", FALSE},
	{"http://www.python.org/channews.rdf", "Python.org", FALSE},
	{"http://www.quotationspage.com/data/mqotd.rss", N_("Quotes of the Day"), FALSE},
	{"http://www.salon.com/feed/RDF/salon_use.rdf", "Salon", FALSE},
	{"http://slashdot.org/slashdot.rdf", "Slashdot", FALSE},
	{"http://www.theregister.co.uk/tonys/slashdot.rdf", "The Register", FALSE},
	{"http://www.thinkgeek.com/thinkgeek.rdf", "Think Geek", FALSE},
	{"http://www.webreference.com/webreference.rdf", "Web Reference", FALSE},
	{"http://redcarpet.ximian.com/red-carpet.rdf", "Ximian Red Carpet New", FALSE},
	{NULL, NULL, FALSE}
};

static void
free_rdf_info (struct _RDFInfo *info)
{
	g_free (info->url);
	g_free (info->name);
	g_free (info);
}

static const char *
find_name_for_url (PropertyData *pd,
		   const char *url)
{
	GList *p;

	for (p = pd->rdf->known; p; p = p->next) {
		struct _RDFInfo *info = p->data;
		
		if (info == NULL || info->url == NULL) {
			continue;
		}

		if (strcmp (url, info->url) == 0) {
			return info->name;
		}
	}

	return url;
}

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

	for (p = pd->summary->preferences->rdf_urls; p; p = p->next) {
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
	ESummaryTableModelEntry *entry;
	ESummaryTable *est;
	FILE *handle;
	int i, total;
	char *rdf_file, line[4096];

	if (pd->rdf->default_hash == NULL) {
		pd->rdf->default_hash = g_hash_table_new (g_str_hash, g_str_equal);
	}

	est = E_SUMMARY_TABLE (widget);

	/* Fill the defaults first */
	for (i = 0; rdfs[i].url; i++) {
		ETreePath path;

		path = e_summary_table_add_node (est, NULL, i, NULL);

		entry = g_new (ESummaryTableModelEntry, 1);
		entry->path = path;
		entry->location = g_strdup (rdfs[i].url);
		entry->name = g_strdup (rdfs[i].name);
		entry->editable = TRUE;
		entry->removable = FALSE;
		entry->shown = rdf_is_shown (pd, entry->location);
		g_hash_table_insert (est->model, entry->path, entry);

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
		ETreePath path;
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

		path = e_summary_table_add_node (est, NULL, total++, NULL);
		entry = g_new (ESummaryTableModelEntry, 1);
		entry->path = path;
		entry->location = g_strdup (info->url);
		entry->name = g_strdup (info->name);
		entry->editable = TRUE;
		entry->removable = TRUE;
		entry->shown = rdf_is_shown (pd, entry->location);
		g_hash_table_insert (est->model, entry->path, entry);
				
		g_strfreev (tokens);
	}

	fclose (handle);
}

static void
fill_weather_etable (ESummaryTable *est,
		     PropertyData *pd)
{
	e_summary_weather_fill_etable (est, pd->summary);
}

static void
fill_mail_etable (ESummaryTable *est,
		  PropertyData *pd)
{
	e_summary_mail_fill_list (est, pd->summary);
}

static void
mail_show_full_path_toggled_cb (GtkToggleButton *tb,
				PropertyData *pd)
{
	pd->summary->preferences->show_full_path = gtk_toggle_button_get_active (tb);

	evolution_config_control_changed (pd->config_control);
}

#if 0
static void
add_dialog_clicked_cb (GnomeDialog *dialog,
		       int button,
		       PropertyData *pd)
{
	struct _RDFInfo *info;
	char *url, *name;
	char *text[1];
	int row;

	if (button == 1) {
		gnome_dialog_close (dialog);
		return;
	}

	url = gtk_entry_get_text (GTK_ENTRY (pd->new_url_entry));
	if (url == NULL || *text == 0) {
		gnome_dialog_close (dialog);
		return;
	}
	name = gtk_entry_get_text (GTK_ENTRY (pd->new_name_entry));
	info = g_new (struct _RDFInfo, 1);
	info->url = g_strdup (url);
	info->name = name ? g_strdup (name) : g_strdup (url);

	text[0] = info->name;
	row = gtk_clist_append (GTK_CLIST (pd->rdf->all), text);
	gtk_clist_set_row_data_full (GTK_CLIST (pd->rdf->all), row, info,
				     (GdkDestroyNotify) free_rdf_info);
	pd->rdf->known = g_list_append (pd->rdf->known, info);

	save_known_rdfs (pd->rdf->known);
	pd->summary->preferences->rdf_urls = g_list_prepend (pd->summary->preferences->rdf_urls, g_strdup (info->url));
	row = gtk_clist_prepend (GTK_CLIST (pd->rdf->shown), text);
	gtk_clist_set_row_data (GTK_CLIST (pd->rdf->shown), row,
				pd->summary->preferences->rdf_urls);

	evolution_config_control_changed (pd->config_control);
	gnome_dialog_close (dialog);
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
#endif

static void
rdf_refresh_value_changed_cb (GtkAdjustment *adj,
			      PropertyData *pd)
{
	pd->summary->preferences->rdf_refresh_time = (int) adj->value;
	evolution_config_control_changed (pd->config_control);
}

static void
rdf_limit_value_changed_cb (GtkAdjustment *adj,
			    PropertyData *pd)
{
	pd->summary->preferences->limit = (int) adj->value;
	evolution_config_control_changed (pd->config_control);
}

static void
mail_etable_item_changed_cb (ESummaryTable *est,
			     ETreePath path,
			     PropertyData *pd)
{
	evolution_config_control_changed (pd->config_control);
}

static void
rdf_etable_item_changed_cb (ESummaryTable *est,
			    ETreePath path,
			    PropertyData *pd)
{
	evolution_config_control_changed (pd->config_control);
}

static void
weather_etable_item_changed_cb (ESummaryTable *est,
				ETreePath path,
				PropertyData *pd)
{
	evolution_config_control_changed (pd->config_control);
}

static void
weather_refresh_value_changed_cb (GtkAdjustment *adj,
				  PropertyData *pd)
{
	pd->summary->preferences->weather_refresh_time = (int) adj->value;
	evolution_config_control_changed (pd->config_control);
}

static void
weather_metric_toggled_cb (GtkToggleButton *tb,
			   PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->units = UNITS_METRIC;
	evolution_config_control_changed (pd->config_control);
}

static void
weather_imperial_toggled_cb (GtkToggleButton *tb,
			     PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->units = UNITS_IMPERIAL;
	evolution_config_control_changed (pd->config_control);
}


static void
calendar_one_toggled_cb (GtkToggleButton *tb,
			 PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->days = E_SUMMARY_CALENDAR_ONE_DAY;
	evolution_config_control_changed (pd->config_control);
}

static void
calendar_five_toggled_cb (GtkToggleButton *tb,
			  PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->days = E_SUMMARY_CALENDAR_FIVE_DAYS;
	evolution_config_control_changed (pd->config_control);
}

static void
calendar_week_toggled_cb (GtkToggleButton *tb,
			  PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->days = E_SUMMARY_CALENDAR_ONE_WEEK;
	evolution_config_control_changed (pd->config_control);
}

static void
calendar_month_toggled_cb (GtkToggleButton *tb,
			   PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->days = E_SUMMARY_CALENDAR_ONE_MONTH;
	evolution_config_control_changed (pd->config_control);
}

static void
calendar_all_toggled_cb (GtkToggleButton *tb,
			 PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->show_tasks = E_SUMMARY_CALENDAR_ALL_TASKS;
	evolution_config_control_changed (pd->config_control);
}

static void
calendar_today_toggled_cb (GtkToggleButton *tb,
			   PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->show_tasks = E_SUMMARY_CALENDAR_TODAYS_TASKS;
	evolution_config_control_changed (pd->config_control);
}

static gboolean
make_property_dialog (PropertyData *pd)
{
	struct _MailPage *mail;
	struct _RDFPage *rdf;
	struct _WeatherPage *weather;
	struct _CalendarPage *calendar;

	/* Mail */
	mail = pd->mail = g_new (struct _MailPage, 1);
	mail->tmp_list = NULL;
	
	mail->etable = glade_xml_get_widget (pd->xml, "mail-custom");
	g_return_val_if_fail (mail->etable != NULL, FALSE);

	gtk_signal_connect (GTK_OBJECT (mail->etable), "item-changed",
			    GTK_SIGNAL_FUNC (mail_etable_item_changed_cb), pd);
	
	mail->model = E_SUMMARY_TABLE (mail->etable)->model;
	fill_mail_etable (E_SUMMARY_TABLE (mail->etable), pd);
	
	mail->fullpath = glade_xml_get_widget (pd->xml, "checkbutton1");
	g_return_val_if_fail (mail->fullpath != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mail->fullpath),
				      pd->summary->preferences->show_full_path);
	gtk_signal_connect (GTK_OBJECT (mail->fullpath), "toggled",
			    GTK_SIGNAL_FUNC (mail_show_full_path_toggled_cb), pd);

	/* RDF */
	rdf = pd->rdf = g_new (struct _RDFPage, 1);
	rdf->known = NULL;
	rdf->tmp_list = NULL;
	rdf->default_hash = NULL;
	
	rdf->etable = glade_xml_get_widget (pd->xml, "rdf-custom");
	g_return_val_if_fail (rdf->etable != NULL, FALSE);

	gtk_signal_connect (GTK_OBJECT (rdf->etable), "item-changed",
			    GTK_SIGNAL_FUNC (rdf_etable_item_changed_cb), pd);

	rdf->model = E_SUMMARY_TABLE (rdf->etable)->model;

	fill_rdf_etable (rdf->etable, pd);
	
	rdf->refresh = glade_xml_get_widget (pd->xml, "spinbutton1");
	g_return_val_if_fail (rdf->refresh != NULL, FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (rdf->refresh),
				   (float) pd->summary->preferences->rdf_refresh_time);
	gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (rdf->refresh)->adjustment), "value_changed",
			    GTK_SIGNAL_FUNC (rdf_refresh_value_changed_cb), pd);

	rdf->limit = glade_xml_get_widget (pd->xml, "spinbutton4");
	g_return_val_if_fail (rdf->limit != NULL, FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (rdf->limit), 
				   (float) pd->summary->preferences->limit);
	gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (rdf->limit)->adjustment), "value_changed",
			    GTK_SIGNAL_FUNC (rdf_limit_value_changed_cb), pd);

	/* Weather */
	weather = pd->weather = g_new (struct _WeatherPage, 1);
	weather->tmp_list = NULL;
	
	weather->etable = glade_xml_get_widget (pd->xml, "weather-custom");
	g_return_val_if_fail (weather->etable != NULL, FALSE);

	gtk_signal_connect (GTK_OBJECT (weather->etable), "item-changed",
			    GTK_SIGNAL_FUNC (weather_etable_item_changed_cb),
			    pd);
	weather->model = E_SUMMARY_TABLE (weather->etable)->model;

	fill_weather_etable (E_SUMMARY_TABLE (weather->etable), pd);

	weather->refresh = glade_xml_get_widget (pd->xml, "spinbutton5");
	g_return_val_if_fail (weather->refresh != NULL, FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (weather->refresh),
				   (float) pd->summary->preferences->weather_refresh_time);
	gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (weather->refresh)->adjustment),
					"value-changed",
					GTK_SIGNAL_FUNC (weather_refresh_value_changed_cb),
					pd);

	weather->metric = glade_xml_get_widget (pd->xml, "radiobutton7");
	g_return_val_if_fail (weather->metric != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (weather->metric), (pd->summary->preferences->units == UNITS_METRIC));
	gtk_signal_connect (GTK_OBJECT (weather->metric), "toggled",
			    GTK_SIGNAL_FUNC (weather_metric_toggled_cb), pd);

	weather->imperial = glade_xml_get_widget (pd->xml, "radiobutton8");
	g_return_val_if_fail (weather->imperial != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (weather->imperial), (pd->summary->preferences->units == UNITS_IMPERIAL));
	gtk_signal_connect (GTK_OBJECT (weather->imperial), "toggled",
			    GTK_SIGNAL_FUNC (weather_imperial_toggled_cb), pd);

	/* Calendar */
	calendar = pd->calendar = g_new (struct _CalendarPage, 1);
	calendar->one = glade_xml_get_widget (pd->xml, "radiobutton3");
	g_return_val_if_fail (calendar->one != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->one),
				      (pd->summary->preferences->days == E_SUMMARY_CALENDAR_ONE_DAY));
	gtk_signal_connect (GTK_OBJECT (calendar->one), "toggled",
			    GTK_SIGNAL_FUNC (calendar_one_toggled_cb), pd);

	calendar->five = glade_xml_get_widget (pd->xml, "radiobutton4");
	g_return_val_if_fail (calendar->five != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->five),
				      (pd->summary->preferences->days == E_SUMMARY_CALENDAR_FIVE_DAYS));
	gtk_signal_connect (GTK_OBJECT (calendar->five), "toggled",
			    GTK_SIGNAL_FUNC (calendar_five_toggled_cb), pd);

	calendar->week = glade_xml_get_widget (pd->xml, "radiobutton5");
	g_return_val_if_fail (calendar->week != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->week),
				      (pd->summary->preferences->days == E_SUMMARY_CALENDAR_ONE_WEEK));
	gtk_signal_connect (GTK_OBJECT (calendar->week), "toggled",
			    GTK_SIGNAL_FUNC (calendar_week_toggled_cb), pd);

	calendar->month = glade_xml_get_widget (pd->xml, "radiobutton6");
	g_return_val_if_fail (calendar->month != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->month),
				      (pd->summary->preferences->days == E_SUMMARY_CALENDAR_ONE_MONTH));
	gtk_signal_connect (GTK_OBJECT (calendar->month), "toggled",
			    GTK_SIGNAL_FUNC (calendar_month_toggled_cb), pd);

	calendar->all = glade_xml_get_widget (pd->xml, "radiobutton1");
	g_return_val_if_fail (calendar->all != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->all),
				      (pd->summary->preferences->show_tasks == E_SUMMARY_CALENDAR_ALL_TASKS));
	gtk_signal_connect (GTK_OBJECT (calendar->all), "toggled",
			    GTK_SIGNAL_FUNC (calendar_all_toggled_cb), pd);

	calendar->today = glade_xml_get_widget (pd->xml, "radiobutton2");
	g_return_val_if_fail (calendar->today != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->today),
				      (pd->summary->preferences->show_tasks == E_SUMMARY_CALENDAR_TODAYS_TASKS));
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
	if (pd->summary) {
		gtk_object_unref (GTK_OBJECT (pd->summary));
	}

	g_free (pd);
}

static void
maybe_add_to_shown (gpointer key,
		    gpointer value,
		    gpointer data)
{
	ESummaryTableModelEntry *item;
	GList **list;

	item = (ESummaryTableModelEntry *) value;
	list = (GList **) data;

	if (item->shown == TRUE) {
		*list = g_list_prepend (*list, g_strdup (item->location));
	}
}


/* Prototypes to shut gcc up */
GtkWidget *e_summary_preferences_make_mail_table (PropertyData *pd);
GtkWidget *e_summary_preferences_make_rdf_table (PropertyData *pd);
GtkWidget *e_summary_preferences_make_weather_table (PropertyData *pd);
GtkWidget *
e_summary_preferences_make_mail_table (PropertyData *pd)
{
	return e_summary_table_new (g_hash_table_new (NULL, NULL));
}

GtkWidget *
e_summary_preferences_make_rdf_table (PropertyData *pd)
{
	return e_summary_table_new (g_hash_table_new (NULL, NULL));
}

GtkWidget *
e_summary_preferences_make_weather_table (PropertyData *pd)
{
	return e_summary_table_new (g_hash_table_new (NULL, NULL));
}


/* The factory for the ConfigControl.  */

static void
config_control_apply_cb (EvolutionConfigControl *control,
			 void *data)
{
	PropertyData *pd;

	pd = (PropertyData *) data;

	/* RDFs */
	if (pd->rdf->tmp_list) {
		free_str_list (pd->rdf->tmp_list);
		g_list_free (pd->rdf->tmp_list);
		pd->rdf->tmp_list = NULL;
	}

	/* Take each news feed which is on and add it
	   to the shown list */
	g_hash_table_foreach (pd->rdf->model, maybe_add_to_shown, &pd->rdf->tmp_list);

	if (pd->summary->preferences->rdf_urls) {
		free_str_list (pd->summary->preferences->rdf_urls);
		g_list_free (pd->summary->preferences->rdf_urls);
	}

	pd->summary->preferences->rdf_urls = copy_str_list (pd->rdf->tmp_list);

	/* Weather */
	if (pd->weather->tmp_list) {
		free_str_list (pd->weather->tmp_list);
		g_list_free (pd->weather->tmp_list);
		pd->weather->tmp_list = NULL;
	}
	g_hash_table_foreach (pd->weather->model, maybe_add_to_shown, &pd->weather->tmp_list);
		
	if (pd->summary->preferences->stations) {
		free_str_list (pd->summary->preferences->stations);
		g_list_free (pd->summary->preferences->stations);
	}
	pd->summary->preferences->stations = copy_str_list (pd->weather->tmp_list);
		
	/* Folders */
	if (pd->mail->tmp_list) {
		free_str_list (pd->mail->tmp_list);
		g_list_free (pd->mail->tmp_list);
		pd->mail->tmp_list = NULL;
	}
	g_hash_table_foreach (pd->mail->model, maybe_add_to_shown, &pd->mail->tmp_list);

	if (pd->summary->preferences->display_folders) {
		free_str_list (pd->summary->preferences->display_folders);
		g_list_free (pd->summary->preferences->display_folders);
	}
	pd->summary->preferences->display_folders = copy_str_list (pd->mail->tmp_list);
		
	e_summary_reconfigure (pd->summary);
}

static void
config_control_destroy_cb (EvolutionConfigControl *config_control,
			   void *data)
{
	PropertyData *pd;

	pd = (PropertyData *) data;

	if (pd->summary->old_prefs != NULL) {
		e_summary_preferences_free (pd->summary->old_prefs);
		pd->summary->old_prefs = NULL;
	}

	e_summary_preferences_save (pd->summary->preferences);
	free_property_dialog (pd);
}

static BonoboObject *
factory_fn (BonoboGenericFactory *generic_factory,
	    void *data)
{
	ESummary *summary;
	PropertyData *pd;
	GtkWidget *widget;

	summary = E_SUMMARY (data);

	pd = g_new0 (PropertyData, 1);

	gtk_object_ref (GTK_OBJECT (summary));
	pd->summary = summary;

	if (summary->old_prefs != NULL)
		e_summary_preferences_free (summary->old_prefs);

	summary->old_prefs = e_summary_preferences_copy (summary->preferences);

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
e_summary_preferences_register_config_control_factory (ESummary *summary)
{
	if (bonobo_generic_factory_new (FACTORY_ID, factory_fn, summary) == NULL)
		return FALSE;

	return TRUE;
}
