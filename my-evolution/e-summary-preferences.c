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

#include "e-summary.h"
#include "e-summary-preferences.h"

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
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo-conf/bonobo-config-database.h>

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
	GtkWidget *all, *shown;
	GtkWidget *fullpath;
	GtkWidget *add, *remove;
};

struct _RDFPage {
	GtkWidget *all, *shown;
	GtkWidget *refresh, *limit;
	GtkWidget *add, *remove;
	GtkWidget *new_url;

	GList *known;
};

struct _WeatherPage {
	GtkWidget *all, *shown;
	GtkWidget *refresh, *imperial, *metric;
	GtkWidget *add, *remove;

	GtkCTreeNode *selected_node;
};

struct _CalendarPage {
	GtkWidget *one, *five, *week, *month;
	GtkWidget *all, *today;
};

typedef struct _PropertyData {
	ESummary *summary;
	GnomePropertyBox *box;
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
};

static struct _RDFInfo rdfs[] = {
	{"http://advogato.org/rss/articles.xml", "Advogato"},
	{"http://barrapunto.com/barrapunto.rdf", "Barrapunto"},
	{"http://barrapunto.com/gnome.rdf", "Barrapunto GNOME"},
	{"http://www.bsdtoday.com/backend/bt.rdf", "BSD Today"},
	{"http://beyond2000.com/b2k.rdf", "Beyond 2000"},
	{"http://www.cnn.com/cnn.rss", "CNN"},
        {"http://www.debianplanet.org/debianplanet/backend.php", "Debian Planet"},
	{"http://www.dictionary.com/wordoftheday/wotd.rss", N_("Dictionary.com Word of the Day")},
	{"http://www.dvdreview.com/rss/newschannel.rss", "DVD Review"},
	{"http://freshmeat.net/backend/fm.rdf", "Freshmeat"},
	{"http://news.gnome.org/gnome-news/rdf", "GNotices"},
	{"http://headlines.internet.com/internetnews/prod-news/news.rss", "Internet.com"},
	{"http://www.hispalinux.es/backend.php", "HispaLinux"},
	{"http://dot.kde.org/rdf", "KDE Dot News"},
	{"http://www.kuro5hin.org/backend.rdf", "Kuro5hin"},
	{"http://linuxgames.com/bin/mynetscape.pl", "Linux Games"},
	{"http://linux.com/mrn/jobs/latest_jobs.rss", "Linux Jobs"},
	{"http://linuxtoday.com/backend/my-netscape.rdf", "Linux Today"},
	{"http://lwn.net/headlines/rss", "Linux Weekly News"},
	{"http://www.linux.com/mrn/front_page.rss", "Linux.com"},
	{"http://memepool.com/memepool.rss", "Memepool"},
	{"http://www.mozilla.org/news.rdf", "Mozilla"},
	{"http://www.mozillazine.org/contents.rdf", "Mozillazine"},
	{"http://www.fool.com/about/headlines/rss_headlines.asp", "The Motley Fool"},
	{"http://www.newsforge.com/newsforge.rss", "Newsforge"},
	{"http://www.nanotechnews.com/nano/rdf", "Nanotech News"},
	{"http://www.pigdog.org/pigdog.rdf", "Pigdog"},
	{"http://www.python.org/channews.rdf", "Python.org"},
	{"http://www.quotationspage.com/data/mqotd.rss", N_("Quotes of the Day")},
	{"http://www.salon.com/feed/RDF/salon_use.rdf", "Salon"},
	{"http://slashdot.org/slashdot.rdf", "Slashdot"},
	{"http://www.theregister.co.uk/tonys/slashdot.rdf", "The Register"},
	{"http://www.thinkgeek.com/thinkgeek.rdf", "Think Geek"},
	{"http://www.webreference.com/webreference.rdf", "Web Reference"},
	{"http://redcarpet.ximian.com/red-carpet.rdf", "Ximian Red Carpet New"},
	{NULL, NULL}
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
		line = g_strconcat (info->url, ",", info->name, "\n", NULL);
		fputs (line, handle);
		g_free (line);
	}

	fclose (handle);
}

static void
fill_rdf_all_clist (GtkCList *clist, 
		    PropertyData *pd)
{
	FILE *handle;
	int i;
	char *rdf_file, line[4096];

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
		for (i = 0; rdfs[i].url; i++) {
			char *text[1];
			int row;
			
			text[0] = _(rdfs[i].name);
			row = gtk_clist_append (clist, text);
			/* We don't need to free this data as it's
			   static */
			gtk_clist_set_row_data (clist, row, &rdfs[i]);
			pd->rdf->known = g_list_append (pd->rdf->known, &rdfs[i]);
		}
		
		return;
	}

	while (fgets (line, 4095, handle)) {
		char *text[1];
		char **tokens;
		struct _RDFInfo *info;
		int row;

		if (line[strlen (line) - 1] == '\n') {
			line[strlen (line) - 1] = 0;
		}

		tokens = g_strsplit (line, ",", 2);
		if (tokens == NULL) {
			continue;
		}

		info = g_new (struct _RDFInfo, 1);
		info->url = g_strdup (tokens[0]);
		info->name = g_strdup (tokens[1]);

		pd->rdf->known = g_list_append (pd->rdf->known, info);
		text[0] = tokens[1];
		row = gtk_clist_append (clist, text);
		gtk_clist_set_row_data_full (clist, row, info,
					     (GtkDestroyNotify) free_rdf_info);
		g_strfreev (tokens);
	}

	fclose (handle);
}

static void
fill_rdf_shown_clist (GtkCList *clist,
		      PropertyData *pd)
{
	GList *p;

	for (p = pd->summary->preferences->rdf_urls; p; p = p->next) {
		char *text[1];
		int row;

		text[0] = (char *) find_name_for_url (pd, p->data);
		row = gtk_clist_append (clist, text);
		gtk_clist_set_row_data (clist, row, p);
	}
}

static void
fill_weather_all_ctree (GtkCTree *ctree)
{
	e_summary_weather_ctree_fill (ctree);
}

static void
fill_weather_shown_clist (GtkCList *clist,
			  PropertyData *pd)
{
	GList *p;

	for (p = pd->summary->preferences->stations; p; p = p->next) {
		char *text[1];
		char *pretty;

		pretty = (char *) e_summary_weather_code_to_name (p->data);

		text[0] = pretty;
		gtk_clist_append (clist, text);
	}
}

static void
fill_mail_all_clist (GtkCList *clist,
		     PropertyData *pd)
{
	e_summary_mail_fill_list (clist, pd->summary);
}

static void
fill_mail_shown_clist (GtkCList *clist,
		       PropertyData *pd)
{
	GList *p;

	for (p = pd->summary->preferences->display_folders; p; p = p->next) {
		char *text[1];
		char *uri;
		const char *name;
		int row;

		uri = g_strconcat ("file://", p->data, NULL);
		name = e_summary_mail_uri_to_name (pd->summary, uri);
		g_free (uri);
		if (name == NULL) {
			text[0] = p->data;
		} else {
			text[0] = (char *) name + 1; /* GtkCList sucks.  */
		}
		row = gtk_clist_prepend (clist, text);
		gtk_clist_set_row_data (clist, row, p);
	}
}

static void
mail_all_select_row_cb (GtkCList *clist,
			int row,
			int column,
			GdkEvent *event,
			PropertyData *pd)
{
	ESummaryMailRowData *rd;
	GList *p;

	rd = gtk_clist_get_row_data (GTK_CLIST (pd->mail->all), row);
	if (rd == NULL) {
		return;
	}

	for (p = pd->summary->preferences->display_folders; p; p = p->next) {
		if (strcmp (rd->uri + 7, p->data) == 0) {
			/* Already in list */
			return;
		}
	}

	gtk_widget_set_sensitive (pd->mail->add, TRUE);
}

static void
mail_all_unselect_row_cb (GtkCList *clist,
			  int row,
			  int column,
			  GdkEvent *event,
			  PropertyData *pd)
{
	if (clist->selection == NULL) {
		gtk_widget_set_sensitive (pd->mail->add, FALSE);
	}
}

static void
mail_shown_select_row_cb (GtkCList *clist,
			  int row,
			  int column,
			  GdkEvent *event,
			  PropertyData *pd)
{
	gtk_widget_set_sensitive (pd->mail->remove, TRUE);
}

static void
mail_shown_unselect_row_cb (GtkCList *clist,
			    int row,
			    int column,
			    GdkEvent *event,
			    PropertyData *pd)
{
	if (clist->selection == NULL) {
		gtk_widget_set_sensitive (pd->mail->remove, FALSE);
	}
}

static void
mail_add_clicked_cb (GtkButton *button,
		     PropertyData *pd)
{
	int row;
	ESummaryMailRowData *rd;
	char *text[1];
	GList *p;
	
	row = GPOINTER_TO_INT (GTK_CLIST (pd->mail->all)->selection->data);
	rd = gtk_clist_get_row_data (GTK_CLIST (pd->mail->all), row);
	g_return_if_fail (rd != NULL);

	for (p = pd->summary->preferences->display_folders; p; p = p->next) {
		if (strcmp (rd->uri + 7, p->data) == 0) {
			/* Already in list */
			return;
		}
	}

	text[0] = rd->name + 1;
	row = gtk_clist_append (GTK_CLIST (pd->mail->shown), text);

	pd->summary->preferences->display_folders = g_list_prepend (pd->summary->preferences->display_folders,
								   g_strdup (rd->uri + 7));
	gtk_clist_set_row_data (GTK_CLIST (pd->mail->shown), row, pd->summary->preferences->display_folders);

	gnome_property_box_changed (pd->box);
}

static void
mail_remove_clicked_cb (GtkButton *button,
			PropertyData *pd)
{
	int row;
	GList *p;

	row = GPOINTER_TO_INT (GTK_CLIST (pd->mail->shown)->selection->data);
	p = gtk_clist_get_row_data (GTK_CLIST (pd->mail->shown), row);

	gtk_clist_remove (GTK_CLIST (pd->mail->shown), row);
	pd->summary->preferences->display_folders = g_list_remove_link (pd->summary->preferences->display_folders, p);
	g_free (p->data);
	g_list_free (p);

	gtk_clist_select_row (GTK_CLIST (pd->mail->shown), row, 0);
	gnome_property_box_changed (pd->box);
}

static void
mail_show_full_path_toggled_cb (GtkToggleButton *tb,
				PropertyData *pd)
{
	pd->summary->preferences->show_full_path = gtk_toggle_button_get_active (tb);
	gnome_property_box_changed (pd->box);
}

static void
rdf_all_select_row_cb (GtkCList *clist,
		       int row,
		       int column,
		       GdkEvent *event,
		       PropertyData *pd)
{
	struct _RDFInfo *info;
	GList *p;

	info = gtk_clist_get_row_data (GTK_CLIST (pd->rdf->all), row);
	g_return_if_fail (info != NULL);

	for (p = pd->summary->preferences->rdf_urls; p; p = p->next) {
		if (strcmp (p->data, info->url) == 0) {
				/* Found it already */
			return;
		}
	}

	gtk_widget_set_sensitive (pd->rdf->add, TRUE);
}

static void
rdf_all_unselect_row_cb (GtkCList *clist,
			 int row,
			 int column,
			 GdkEvent *event,
			 PropertyData *pd)
{
	if (GTK_CLIST (pd->rdf->all)->selection == NULL) {
		gtk_widget_set_sensitive (pd->rdf->add, FALSE);
	}
}

static void
rdf_shown_select_row_cb (GtkCList *clist,
			 int row,
			 int column,
			 GdkEvent *event,
			 PropertyData *pd)
{
	gtk_widget_set_sensitive (pd->rdf->remove, TRUE);
}

static void
rdf_shown_unselect_row_cb (GtkCList *clist,
			   int row,
			   int column,
			   GdkEvent *event,
			   PropertyData *pd)
{
	if (GTK_CLIST (pd->rdf->shown)->selection == NULL) {
		gtk_widget_set_sensitive (pd->rdf->remove, FALSE);
	}
}

static void
rdf_add_clicked_cb (GtkButton *button,
		    PropertyData *pd)
{
	struct _RDFInfo *info;
	GList *p, *rows;
	char *text[1];
	int row;
	
	for (rows = GTK_CLIST (pd->rdf->all)->selection; rows; rows = rows->next) {
		row = GPOINTER_TO_INT (rows->data);
		info = gtk_clist_get_row_data (GTK_CLIST (pd->rdf->all), row);
		
		text[0] = info->name;
		
		for (p = pd->summary->preferences->rdf_urls; p; p = p->next) {
			if (strcmp (p->data, info->url) == 0) {
				/* Found it already */
				return;
			}
		}

		
		pd->summary->preferences->rdf_urls = g_list_prepend (pd->summary->preferences->rdf_urls, g_strdup (info->url));
		row = gtk_clist_prepend (GTK_CLIST (pd->rdf->shown), text);
		gtk_clist_set_row_data (GTK_CLIST (pd->rdf->shown), row,
					pd->summary->preferences->rdf_urls);
	}
	gnome_property_box_changed (pd->box);
}

static void
rdf_remove_clicked_cb (GtkButton *button,
		       PropertyData *pd)
{
	GList *p;
	int row;

	row = GPOINTER_TO_INT (GTK_CLIST (pd->rdf->shown)->selection->data);
	p = gtk_clist_get_row_data (GTK_CLIST (pd->rdf->shown), row);
	gtk_clist_remove (GTK_CLIST (pd->rdf->shown), row);

	pd->summary->preferences->rdf_urls = g_list_remove_link (pd->summary->preferences->rdf_urls, p);
	g_free (p->data);
	g_list_free (p);

	gtk_clist_select_row (GTK_CLIST (pd->rdf->shown), row, 0);
	gnome_property_box_changed (pd->box);
}

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


	gnome_property_box_changed (pd->box);
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

static void
rdf_refresh_value_changed_cb (GtkAdjustment *adj,
			      PropertyData *pd)
{
	pd->summary->preferences->rdf_refresh_time = (int) adj->value;
	gnome_property_box_changed (pd->box);
}

static void
rdf_limit_value_changed_cb (GtkAdjustment *adj,
			    PropertyData *pd)
{
	pd->summary->preferences->limit = (int) adj->value;
	gnome_property_box_changed (pd->box);
}

static void
weather_all_select_row_cb (GtkCTree *ctree,
			   GtkCTreeNode *row,
			   int column,
			   PropertyData *pd)
{
	ESummaryWeatherLocation *location;
	GList *p;

	location = gtk_ctree_node_get_row_data (GTK_CTREE (pd->weather->all), row);
	if (location == NULL) {
		gtk_ctree_unselect (ctree, row);
		return;
	}

	for (p = pd->summary->preferences->stations; p; p = p->next) {
		if (strcmp (location->code, p->data) == 0) {
			return; /* Already have it */
		}
	}

	gtk_widget_set_sensitive (pd->weather->add, TRUE);
	pd->weather->selected_node = row;
}

static void
weather_all_unselect_row_cb (GtkCList *clist,
			     GtkCTreeNode *row,
			     int column,
			     PropertyData *pd)
{
	if (clist->selection == NULL) {
		gtk_widget_set_sensitive (pd->weather->add, FALSE);
	}
	pd->weather->selected_node = NULL;
}

static void
weather_shown_select_row_cb (GtkCList *clist,
			     int row,
			     int column,
			     GdkEvent *event,
			     PropertyData *pd)
{
	gtk_widget_set_sensitive (pd->weather->remove, TRUE);
}

static void
weather_shown_unselect_row_cb (GtkCList *clist,
			       int row,
			       int column,
			       GdkEvent *event,
			       PropertyData *pd)
{
	if (clist->selection == NULL) {
		gtk_widget_set_sensitive (pd->weather->remove, FALSE);
	}
}

static void
weather_add_clicked_cb (GtkButton *button,
			PropertyData *pd)
{
	ESummaryWeatherLocation *location;
	GList *p;
	char *text[1];

	location = gtk_ctree_node_get_row_data (GTK_CTREE (pd->weather->all), pd->weather->selected_node);

	g_return_if_fail (location != NULL);

	for (p = pd->summary->preferences->stations; p; p = p->next) {
		if (strcmp (location->code, p->data) == 0) {
			return; /* Already have it */
		}
	}

	pd->summary->preferences->stations = g_list_prepend (pd->summary->preferences->stations, g_strdup (location->code));
	text[0] = location->name;
	gtk_clist_prepend (GTK_CLIST (pd->weather->shown), text);

	gnome_property_box_changed (pd->box);
}

static void
weather_remove_clicked_cb (GtkButton *button,
			   PropertyData *pd)
{
	int row;
	GList *p;

	row = GPOINTER_TO_INT (GTK_CLIST (pd->weather->shown)->selection->data);
	p = g_list_nth (pd->summary->preferences->stations, row);
	gtk_clist_remove (GTK_CLIST (pd->weather->shown), row);

	pd->summary->preferences->stations = g_list_remove_link (pd->summary->preferences->stations, p);
	g_free (p->data);
	g_list_free (p);

	gtk_clist_select_row (GTK_CLIST (pd->weather->shown), row, 0);
	gnome_property_box_changed (pd->box);
}

static void
weather_refresh_value_changed_cb (GtkAdjustment *adj,
				  PropertyData *pd)
{
	pd->summary->preferences->weather_refresh_time = (int) adj->value;
	gnome_property_box_changed (pd->box);
}

static void
weather_metric_toggled_cb (GtkToggleButton *tb,
			   PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->units = UNITS_METRIC;
	gnome_property_box_changed (pd->box);
}

static void
weather_imperial_toggled_cb (GtkToggleButton *tb,
			     PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->units = UNITS_IMPERIAL;
	gnome_property_box_changed (pd->box);
}


static void
calendar_one_toggled_cb (GtkToggleButton *tb,
			 PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->days = E_SUMMARY_CALENDAR_ONE_DAY;
	gnome_property_box_changed (pd->box);
}

static void
calendar_five_toggled_cb (GtkToggleButton *tb,
			  PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->days = E_SUMMARY_CALENDAR_FIVE_DAYS;
	gnome_property_box_changed (pd->box);
}

static void
calendar_week_toggled_cb (GtkToggleButton *tb,
			  PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->days = E_SUMMARY_CALENDAR_ONE_WEEK;
	gnome_property_box_changed (pd->box);
}

static void
calendar_month_toggled_cb (GtkToggleButton *tb,
			   PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->days = E_SUMMARY_CALENDAR_ONE_MONTH;
	gnome_property_box_changed (pd->box);
}

static void
calendar_all_toggled_cb (GtkToggleButton *tb,
			 PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->show_tasks = E_SUMMARY_CALENDAR_ALL_TASKS;
	gnome_property_box_changed (pd->box);
}

static void
calendar_today_toggled_cb (GtkToggleButton *tb,
			   PropertyData *pd)
{
	if (gtk_toggle_button_get_active (tb) == FALSE) {
		return;
	}

	pd->summary->preferences->show_tasks = E_SUMMARY_CALENDAR_TODAYS_TASKS;
	gnome_property_box_changed (pd->box);
}

static void
construct_pixmap_button (GladeXML *xml,
			 const char *id,
			 const char *image)
{
	GtkWidget *box, *pixmap;

	box = glade_xml_get_widget (xml, id);
	
	pixmap = gnome_stock_pixmap_widget (NULL, image);
	gtk_box_pack_start (GTK_BOX (box), pixmap, TRUE, TRUE, 0);

	gtk_widget_show (pixmap);
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

	/* I think this should be a fancy bonobo thingy */
	mail->all = glade_xml_get_widget (pd->xml, "clist7");
	g_return_val_if_fail (mail->all != NULL, FALSE);
	fill_mail_all_clist (GTK_CLIST (mail->all), pd);
	gtk_signal_connect (GTK_OBJECT (mail->all), "select-row",
			    GTK_SIGNAL_FUNC (mail_all_select_row_cb), pd);
	gtk_signal_connect (GTK_OBJECT (mail->all), "unselect-row",
			    GTK_SIGNAL_FUNC (mail_all_unselect_row_cb), pd);

	mail->shown = glade_xml_get_widget (pd->xml, "clist1");
	g_return_val_if_fail (mail->shown != NULL, FALSE);
	fill_mail_shown_clist (GTK_CLIST (mail->shown), pd);
	gtk_signal_connect (GTK_OBJECT (mail->shown), "select-row",
			    GTK_SIGNAL_FUNC (mail_shown_select_row_cb), pd);
	gtk_signal_connect (GTK_OBJECT (mail->shown), "unselect-row",
			    GTK_SIGNAL_FUNC (mail_shown_unselect_row_cb), pd);

	mail->fullpath = glade_xml_get_widget (pd->xml, "checkbutton1");
	g_return_val_if_fail (mail->fullpath != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mail->fullpath),
				      pd->summary->preferences->show_full_path);
	gtk_signal_connect (GTK_OBJECT (mail->fullpath), "toggled",
			    GTK_SIGNAL_FUNC (mail_show_full_path_toggled_cb), pd);

	mail->add = glade_xml_get_widget (pd->xml, "button4");
	g_return_val_if_fail (mail->add != NULL, FALSE);
	construct_pixmap_button (pd->xml, "hbox-mailadd", 
				 GNOME_STOCK_BUTTON_NEXT);
	gtk_signal_connect (GTK_OBJECT (mail->add), "clicked",
			    GTK_SIGNAL_FUNC (mail_add_clicked_cb), pd);

	mail->remove = glade_xml_get_widget (pd->xml, "button5");
	g_return_val_if_fail (mail->remove != NULL, FALSE);
	construct_pixmap_button (pd->xml, "hbox-mailremove", 
				 GNOME_STOCK_BUTTON_PREV);
	gtk_signal_connect (GTK_OBJECT (mail->remove), "clicked",
			    GTK_SIGNAL_FUNC (mail_remove_clicked_cb), pd);
	
	/* RDF */
	rdf = pd->rdf = g_new (struct _RDFPage, 1);
	rdf->known = NULL;
	rdf->all = glade_xml_get_widget (pd->xml, "clist6");
	g_return_val_if_fail (rdf->all != NULL, FALSE);
	gtk_signal_connect (GTK_OBJECT (rdf->all), "select-row",
			    GTK_SIGNAL_FUNC (rdf_all_select_row_cb), pd);
	gtk_signal_connect (GTK_OBJECT (rdf->all), "unselect-row",
			    GTK_SIGNAL_FUNC (rdf_all_unselect_row_cb), pd);
	fill_rdf_all_clist (GTK_CLIST (rdf->all), pd);

	rdf->shown = glade_xml_get_widget (pd->xml, "clist5");
	g_return_val_if_fail (rdf->shown != NULL, FALSE);
	gtk_signal_connect (GTK_OBJECT (rdf->shown), "select-row",
			    GTK_SIGNAL_FUNC (rdf_shown_select_row_cb), pd);
	gtk_signal_connect (GTK_OBJECT (rdf->shown), "unselect-row",
			    GTK_SIGNAL_FUNC (rdf_shown_unselect_row_cb), pd);
	fill_rdf_shown_clist (GTK_CLIST (rdf->shown), pd);

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

	rdf->add = glade_xml_get_widget (pd->xml, "button9");
	g_return_val_if_fail (rdf->add != NULL, FALSE);

	construct_pixmap_button (pd->xml, "hbox-newsadd", 
				 GNOME_STOCK_BUTTON_NEXT);
	gtk_widget_set_sensitive (rdf->add, FALSE);
	gtk_signal_connect (GTK_OBJECT (rdf->add), "clicked",
			    GTK_SIGNAL_FUNC (rdf_add_clicked_cb), pd);

	rdf->remove = glade_xml_get_widget (pd->xml, "button10");
	g_return_val_if_fail (rdf->remove != NULL, FALSE);

	construct_pixmap_button (pd->xml, "hbox-newsremove", 
				 GNOME_STOCK_BUTTON_PREV);
	gtk_widget_set_sensitive (rdf->remove, FALSE);
	gtk_signal_connect (GTK_OBJECT (rdf->remove), "clicked",
			    GTK_SIGNAL_FUNC (rdf_remove_clicked_cb), pd);
	
	rdf->new_url = glade_xml_get_widget (pd->xml, "button11");
	g_return_val_if_fail (rdf->new_url != NULL, FALSE);
	gtk_signal_connect (GTK_OBJECT (rdf->new_url), "clicked",
			    GTK_SIGNAL_FUNC (rdf_new_url_clicked_cb), pd);

	/* Weather */
	weather = pd->weather = g_new (struct _WeatherPage, 1);
	weather->all = glade_xml_get_widget (pd->xml, "ctree1");
	g_return_val_if_fail (weather->all != NULL, FALSE);
	fill_weather_all_ctree (GTK_CTREE (weather->all));
	gtk_signal_connect (GTK_OBJECT (weather->all), "tree-select-row",
			    GTK_SIGNAL_FUNC (weather_all_select_row_cb), pd);
	gtk_signal_connect (GTK_OBJECT (weather->all), "tree-unselect-row",
			    GTK_SIGNAL_FUNC (weather_all_unselect_row_cb), pd);

	weather->shown = glade_xml_get_widget (pd->xml, "clist3");
	g_return_val_if_fail (weather->shown != NULL, FALSE);
	fill_weather_shown_clist (GTK_CLIST (weather->shown), pd);
	gtk_signal_connect (GTK_OBJECT (weather->shown), "select-row",
			    GTK_SIGNAL_FUNC (weather_shown_select_row_cb), pd);
	gtk_signal_connect (GTK_OBJECT (weather->shown), "unselect-row",
			    GTK_SIGNAL_FUNC (weather_shown_unselect_row_cb), pd);

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

	weather->add = glade_xml_get_widget (pd->xml, "button6");
	g_return_val_if_fail (weather->add != NULL, FALSE);

	construct_pixmap_button (pd->xml, "hbox-weatheradd", 
				 GNOME_STOCK_BUTTON_NEXT);
	gtk_signal_connect (GTK_OBJECT (weather->add), "clicked",
			    GTK_SIGNAL_FUNC (weather_add_clicked_cb), pd);

	weather->remove = glade_xml_get_widget (pd->xml, "button7");
	g_return_val_if_fail (weather->remove != NULL, FALSE);
	
	construct_pixmap_button (pd->xml, "hbox-weatherremove", 
				 GNOME_STOCK_BUTTON_PREV);
	gtk_signal_connect (GTK_OBJECT (weather->remove), "clicked",
			    GTK_SIGNAL_FUNC (weather_remove_clicked_cb), pd);

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
		g_free (pd->rdf);
	}
	if (pd->mail) {
		g_free (pd->mail);
	}
	if (pd->weather) {
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
property_box_clicked_cb (GnomeDialog *dialog,
			 int page_num,
			 PropertyData *pd)
{
	if (page_num == -1) {
		e_summary_reconfigure (pd->summary);
	}
}

static void
property_box_destroy_cb (GtkObject *object,
			 PropertyData *pd)
{
	if (pd->summary->old_prefs != NULL) {
		e_summary_preferences_free (pd->summary->old_prefs);
		pd->summary->old_prefs = NULL;
	}

	e_summary_preferences_save (pd->summary->preferences);
	pd->summary->prefs_window = NULL;
	free_property_dialog (pd);
}

void
e_summary_configure (BonoboUIComponent *component,
		     gpointer userdata,
		     const char *cname)
{
	ESummary *summary = userdata;
	PropertyData *pd;

	if (summary->prefs_window != NULL) {
		gdk_window_raise (summary->prefs_window->window);
		gdk_window_show (summary->prefs_window->window);
		return;
	}

	pd = g_new0 (PropertyData, 1);

	gtk_object_ref (GTK_OBJECT (summary));
	pd->summary = summary;

	if (summary->old_prefs != NULL) {
		e_summary_preferences_free (summary->old_prefs);
	}

	summary->old_prefs = e_summary_preferences_copy (summary->preferences);

	pd->xml = glade_xml_new (EVOLUTION_GLADEDIR "/my-evolution.glade", NULL);
	g_return_if_fail (pd->xml != NULL);

	pd->box = GNOME_PROPERTY_BOX (glade_xml_get_widget (pd->xml, "dialog1"));
	gtk_widget_hide (pd->box->help_button);
	summary->prefs_window = GTK_WIDGET (pd->box);

	gtk_window_set_title (GTK_WINDOW (pd->box), _("Summary Settings"));
	if (make_property_dialog (pd) == FALSE) {
		g_warning ("Missing some part of XML file");
		free_property_dialog (pd);
		return;
	}

	gtk_signal_connect (GTK_OBJECT (pd->box), "apply",
			    GTK_SIGNAL_FUNC (property_box_clicked_cb), pd);
	gtk_signal_connect (GTK_OBJECT (pd->box), "destroy",
			    GTK_SIGNAL_FUNC (property_box_destroy_cb), pd);
	gtk_widget_show (GTK_WIDGET (pd->box));
}

