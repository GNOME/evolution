/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-preferences.c
 *
 * Copyright (C) 2001, 2002 Ximian, Inc.
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

#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-config.h>

#include <libgnomeui/gnome-propertybox.h>

#include <glade/glade.h>
#include <stdio.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>

#include <gconf/gconf-client.h>

#include <shell/evolution-storage-set-view-listener.h>

#include <string.h>

#include "e-summary.h"
#include "e-summary-preferences.h"
#include "e-summary-table.h"
#include "e-summary-shown.h"

#include "evolution-config-control.h"

static ESummaryPrefs *global_preferences = NULL;
static GNOME_Evolution_Shell global_shell = NULL;

gboolean
e_summary_preferences_restore (ESummaryPrefs *prefs)
{
	GConfClient *gconf_client;
	GSList *path_list;
	GSList *uri_list;
	GSList *p, *q;

	g_return_val_if_fail (prefs != NULL, FALSE);

	gconf_client = gconf_client_get_default ();

	path_list = gconf_client_get_list (gconf_client, "/apps/evolution/summary/mail/folder_evolution_uris",
					   GCONF_VALUE_STRING, NULL);
	uri_list = gconf_client_get_list (gconf_client, "/apps/evolution/summary/mail/folder_physical_uris",
					  GCONF_VALUE_STRING, NULL);

	prefs->display_folders = NULL;
	for (p = path_list, q = uri_list; p != NULL && q != NULL; p = p->next, q = q->next) {
		ESummaryPrefsFolder *folder;

		folder = g_new (ESummaryPrefsFolder, 1);
		folder->evolution_uri = p->data;
		folder->physical_uri = q->data;
		prefs->display_folders = g_slist_append(prefs->display_folders, folder);
	}

	g_slist_free (path_list);
	g_slist_free (uri_list);

	prefs->show_full_path = gconf_client_get_bool (gconf_client, "/apps/evolution/summary/mail/show_full_paths", NULL);

	prefs->rdf_urls = gconf_client_get_list (gconf_client, "/apps/evolution/summary/rdf/uris",
						 GCONF_VALUE_STRING, NULL);

	prefs->rdf_refresh_time = gconf_client_get_int (gconf_client, "/apps/evolution/summary/rdf/refresh_time", NULL);

	prefs->limit = gconf_client_get_int (gconf_client, "/apps/evolution/summary/rdf/max_items", NULL);

	prefs->stations = gconf_client_get_list (gconf_client, "/apps/evolution/summary/weather/stations",
						 GCONF_VALUE_STRING, NULL);

	if (gconf_client_get_bool (gconf_client, "/apps/evolution/summary/weather/use_metric", NULL))
		prefs->units = UNITS_METRIC;
	else
		prefs->units = UNITS_IMPERIAL;

	prefs->weather_refresh_time = gconf_client_get_int (gconf_client, "/apps/evolution/summary/weather/refresh_time",
							    NULL);
	
	prefs->days = gconf_client_get_int (gconf_client, "/apps/evolution/summary/calendar/days_shown", NULL);
	if (gconf_client_get_bool (gconf_client, "/apps/evolution/summary/tasks/show_all", NULL))
		prefs->show_tasks = E_SUMMARY_CALENDAR_ALL_TASKS;
	else
		prefs->show_tasks = E_SUMMARY_CALENDAR_ONE_DAY;

	g_object_unref (gconf_client);
	return TRUE;
}

void
e_summary_preferences_save (ESummaryPrefs *prefs)
{
	GConfClient *gconf_client;
	GSList *evolution_uri_list, *physical_uri_list;
	GSList *p;

	gconf_client = gconf_client_get_default ();

	evolution_uri_list = NULL;
	physical_uri_list = NULL;
	for (p = prefs->display_folders; p != NULL; p = p->next) {
		const ESummaryPrefsFolder *folder;

		folder = (const ESummaryPrefsFolder *) p->data;
		evolution_uri_list = g_slist_prepend (evolution_uri_list, folder->evolution_uri);
		physical_uri_list = g_slist_prepend (physical_uri_list, folder->physical_uri);
	}
	evolution_uri_list = g_slist_reverse (evolution_uri_list);
	physical_uri_list = g_slist_reverse (physical_uri_list);

	gconf_client_set_list (gconf_client, "/apps/evolution/summary/mail/folder_evolution_uris",
			       GCONF_VALUE_STRING, evolution_uri_list, NULL);
	gconf_client_set_list (gconf_client, "/apps/evolution/summary/mail/folder_physical_uris",
			       GCONF_VALUE_STRING, physical_uri_list, NULL);

	g_slist_free (evolution_uri_list);
	g_slist_free (physical_uri_list);

	gconf_client_set_bool (gconf_client, "/apps/evolution/summary/mail/show_full_paths", prefs->show_full_path, NULL);

	gconf_client_set_list (gconf_client, "/apps/evolution/summary/rdf/uris",
			       GCONF_VALUE_STRING, prefs->rdf_urls, NULL);

	gconf_client_set_int (gconf_client, "/apps/evolution/summary/rdf/refresh_time", prefs->rdf_refresh_time, NULL);
	gconf_client_set_int (gconf_client, "/apps/evolution/summary/rdf/max_items", prefs->limit, NULL);

	gconf_client_set_list (gconf_client, "/apps/evolution/summary/weather/stations",
			       GCONF_VALUE_STRING, prefs->stations, NULL);

	gconf_client_set_bool (gconf_client, "/apps/evolution/summary/weather/use_metric",
			       prefs->units == UNITS_METRIC, NULL);
	gconf_client_set_int (gconf_client, "/apps/evolution/summary/weather/refresh_time",
			      prefs->weather_refresh_time, NULL);

	gconf_client_set_int (gconf_client, "/apps/evolution/summary/calendar/days_shown",
			      prefs->days, NULL);

	gconf_client_set_bool (gconf_client, "/apps/evolution/summary/tasks/show_all",
			       prefs->show_tasks == E_SUMMARY_CALENDAR_ALL_TASKS, NULL);

	g_object_unref (gconf_client);
}

static void
free_str_list (GSList *list)
{
	for (; list; list = list->next) {
		g_free (list->data);
	}
}

static void
free_folder_list (GSList *list)
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
		g_slist_free (prefs->display_folders);
	}

	if (prefs->rdf_urls) {
		free_str_list (prefs->rdf_urls);
		g_slist_free (prefs->rdf_urls);
	}

	if (prefs->stations) {
		free_str_list (prefs->stations);
		g_slist_free (prefs->stations);
	}

	g_free (prefs);
}

static GSList *
copy_str_list (GSList *list)
{
	GSList *list_copy = NULL;

	for (; list; list = list->next) {
		list_copy = g_slist_prepend (list_copy, g_strdup (list->data));
	}

	list_copy = g_slist_reverse (list_copy);
	return list_copy;
}

static GSList *
copy_folder_list (GSList *list)
{
	GSList *list_copy = NULL;

	for (; list; list = list->next) {
		ESummaryPrefsFolder *f1, *f2;

		f1 = list->data;
		f2 = g_new (ESummaryPrefsFolder, 1);
		f2->evolution_uri = g_strdup (f1->evolution_uri);
		f2->physical_uri = g_strdup (f1->physical_uri);

		list_copy = g_slist_prepend (list_copy, f2);
	}

	list_copy = g_slist_reverse (list_copy);
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
	
	e_summary_preferences_restore (prefs);

	return prefs;
}

ESummaryPrefs *
e_summary_preferences_get_global (void)
{
	g_assert(global_preferences);

	return global_preferences;
}

struct _MailPage {
	GtkWidget *storage_set_view;
	GtkWidget *all, *shown;
	GtkWidget *fullpath;
	GtkWidget *add, *remove;

	GHashTable *model;
	GSList *tmp_list;
};

struct _RDFPage {
	GtkWidget *etable;
	GtkWidget *refresh, *limit;
	GtkWidget *new_button, *delete_url;

	GHashTable *default_hash, *model;
	GSList *known, *tmp_list;
};

struct _WeatherPage {
	GtkWidget *etable;
	GtkWidget *refresh, *imperial, *metric;
	GtkWidget *add, *remove;

	GHashTable *model;
	GSList *tmp_list;
};

struct _CalendarPage {
	GtkWidget *one, *five, *week, *month;
	GtkWidget *all, *today;
};

typedef struct _PropertyData {
	EvolutionConfigControl *config_control;

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
save_known_rdfs (GSList *rdfs)
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
	GSList *p;

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
	e_summary_shown_freeze (ess);

	/* Fill the defaults first */
	for (i = 0; rdfs[i].url; i++) {
		entry = g_new (ESummaryShownModelEntry, 1);
		entry->location = g_strdup (rdfs[i].url);
		entry->name = g_strdup (rdfs[i].name);
		entry->showable = TRUE;
		entry->data = &rdfs[i];
		
		e_summary_shown_add_node (ess, TRUE, entry, NULL, FALSE, NULL);

		if (rdf_is_shown (pd, rdfs[i].url) == TRUE) {
			entry = g_new (ESummaryShownModelEntry, 1);
			entry->location = g_strdup (rdfs[i].url);
			entry->name = g_strdup (rdfs[i].name);
			entry->showable = TRUE;
			entry->data = &rdfs[i];
			
			e_summary_shown_add_node (ess, FALSE, entry, NULL, FALSE, NULL);
		}

		pd->rdf->known = g_slist_append (pd->rdf->known, &rdfs[i]);

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
		e_summary_shown_thaw (ess);
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
		
		pd->rdf->known = g_slist_append (pd->rdf->known, info);

		entry = g_new (ESummaryShownModelEntry, 1);
		entry->location = g_strdup (info->url);
		entry->name = g_strdup (info->name);
		entry->showable = TRUE;
		entry->data = info;
		
		e_summary_shown_add_node (ess, TRUE, entry, NULL, FALSE, NULL);

		if (rdf_is_shown (pd, tokens[0]) == TRUE) {
			entry = g_new (ESummaryShownModelEntry, 1);
			entry->location = g_strdup (info->url);
			entry->name = g_strdup (info->name);
			entry->showable = TRUE;
			entry->data = info;
			
			e_summary_shown_add_node (ess, FALSE, entry, NULL, FALSE, NULL);
		}

		g_strfreev (tokens);
	}

	fclose (handle);
	e_summary_shown_thaw (ess);
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
rdf_new_url_clicked_cb (GtkButton *button,
			PropertyData *pd)
{
	GtkWidget *add_dialog;
	GtkWidget *name_label, *url_label, *table;
	GtkWidget *new_name_entry, *new_url_entry;

	add_dialog = gtk_dialog_new_with_buttons (_("New News Feed"),
						  GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
						  GTK_DIALOG_DESTROY_WITH_PARENT,
						  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						  GTK_STOCK_OK, GTK_RESPONSE_OK,
						  NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (add_dialog), GTK_RESPONSE_OK);

	table = gtk_table_new (2, 2, FALSE);

	name_label = gtk_label_new_with_mnemonic (_("_Name:"));
       	gtk_table_attach (GTK_TABLE (table), name_label, 0, 1, 0, 1, GTK_FILL, 0, 6, 6);

	new_name_entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), new_name_entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 6, 6);

	url_label = gtk_label_new_with_mnemonic (_("_URL:"));
	gtk_table_attach (GTK_TABLE (table), url_label, 0, 1, 1, 2, GTK_FILL, 0, 6, 6);

	new_url_entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), new_url_entry, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 6, 6);


	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (add_dialog)->vbox), table, FALSE, FALSE, 6);
	gtk_widget_show_all (add_dialog);

	if (gtk_dialog_run (GTK_DIALOG (add_dialog)) == GTK_RESPONSE_OK) {
		const char *url;
		const char *name;

		name = gtk_entry_get_text (GTK_ENTRY (new_name_entry));
		url = gtk_entry_get_text (GTK_ENTRY (new_url_entry));

		if (name != NULL && *name != 0 && url != NULL && *url != 0) {
			ESummaryShownModelEntry *entry;
			struct _RDFInfo *info;

			info = g_new (struct _RDFInfo, 1);
			info->url = g_strdup (url);
			info->name = g_strdup (name);
			info->custom = TRUE;

			pd->rdf->known = g_slist_append (pd->rdf->known, info);

			entry = g_new (ESummaryShownModelEntry, 1);
			entry->location = g_strdup (info->url);
			entry->name = g_strdup (info->name);
			entry->showable = TRUE;
			entry->data = info;
			
			e_summary_shown_add_node (E_SUMMARY_SHOWN (pd->rdf->etable), TRUE,
						  entry, NULL, FALSE, NULL);

			/* Should we add to shown? */

			save_known_rdfs (pd->rdf->known);

			evolution_config_control_changed (pd->config_control);
		}
	}

	gtk_widget_destroy (add_dialog);
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
		pd->rdf->known = g_slist_remove (pd->rdf->known, entry->data);
		
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

	listener = g_object_get_data (G_OBJECT (mail->storage_set_view), "listener");
	g_signal_connect (listener, "folder-toggled", G_CALLBACK (storage_set_changed), pd);

	mail->fullpath = glade_xml_get_widget (pd->xml, "checkbutton1");
	g_return_val_if_fail (mail->fullpath != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mail->fullpath),
				      global_preferences->show_full_path);
	g_signal_connect (mail->fullpath, "toggled", G_CALLBACK (mail_show_full_path_toggled_cb), pd);
	
	/* RDF */
	rdf = pd->rdf = g_new0 (struct _RDFPage, 1);
	rdf->etable = glade_xml_get_widget (pd->xml, "rdf-custom");
	g_return_val_if_fail (rdf->etable != NULL, FALSE);

	g_signal_connect (rdf->etable, "item-changed", G_CALLBACK (rdf_etable_item_changed_cb), pd);
	g_signal_connect (rdf->etable, "selection-changed", G_CALLBACK (rdf_etable_selection_cb), pd);
	
	fill_rdf_etable (rdf->etable, pd);
	rdf->refresh = glade_xml_get_widget (pd->xml, "spinbutton1");
	g_return_val_if_fail (rdf->refresh != NULL, FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (rdf->refresh),
				   (float) global_preferences->rdf_refresh_time);
	g_signal_connect (GTK_SPIN_BUTTON (rdf->refresh)->adjustment, "value_changed",
			  G_CALLBACK (rdf_refresh_value_changed_cb), pd);

	rdf->limit = glade_xml_get_widget (pd->xml, "spinbutton4");
	g_return_val_if_fail (rdf->limit != NULL, FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (rdf->limit), 
				   (float) global_preferences->limit);
	g_signal_connect (GTK_SPIN_BUTTON (rdf->limit)->adjustment, "value_changed",
			  G_CALLBACK (rdf_limit_value_changed_cb), pd);

	rdf->new_button = glade_xml_get_widget (pd->xml, "button11");
	g_return_val_if_fail (rdf->limit != NULL, FALSE);
	g_signal_connect (rdf->new_button, "clicked", G_CALLBACK (rdf_new_url_clicked_cb), pd);

	rdf->delete_url = glade_xml_get_widget (pd->xml, "delete-button");
	g_return_val_if_fail (rdf->delete_url != NULL, FALSE);
	g_signal_connect (rdf->delete_url, "clicked", G_CALLBACK (rdf_delete_url_cb), pd);
	
	/* Weather */
	weather = pd->weather = g_new (struct _WeatherPage, 1);
	weather->tmp_list = NULL;
	
	weather->etable = glade_xml_get_widget (pd->xml, "weather-custom");
	g_return_val_if_fail (weather->etable != NULL, FALSE);

	g_signal_connect (weather->etable, "item-changed", G_CALLBACK (weather_etable_item_changed_cb), pd);

	fill_weather_etable (E_SUMMARY_SHOWN (weather->etable), pd);

	weather->refresh = glade_xml_get_widget (pd->xml, "spinbutton5");
	g_return_val_if_fail (weather->refresh != NULL, FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (weather->refresh),
				   (float) global_preferences->weather_refresh_time);
	g_signal_connect (GTK_SPIN_BUTTON (weather->refresh)->adjustment, "value-changed",
			  G_CALLBACK (weather_refresh_value_changed_cb), pd);

	weather->metric = glade_xml_get_widget (pd->xml, "radiobutton7");
	g_return_val_if_fail (weather->metric != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (weather->metric),
				      (global_preferences->units == UNITS_METRIC));
	g_signal_connect (weather->metric, "toggled", G_CALLBACK (weather_metric_toggled_cb), pd);

	weather->imperial = glade_xml_get_widget (pd->xml, "radiobutton8");
	g_return_val_if_fail (weather->imperial != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (weather->imperial),
				      (global_preferences->units == UNITS_IMPERIAL));
	g_signal_connect (weather->imperial, "toggled", G_CALLBACK (weather_imperial_toggled_cb), pd);

	/* Calendar */
	calendar = pd->calendar = g_new (struct _CalendarPage, 1);
	calendar->one = glade_xml_get_widget (pd->xml, "radiobutton3");
	g_return_val_if_fail (calendar->one != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->one),
				      (global_preferences->days == E_SUMMARY_CALENDAR_ONE_DAY));
	g_signal_connect (calendar->one, "toggled", G_CALLBACK (calendar_one_toggled_cb), pd);

	calendar->five = glade_xml_get_widget (pd->xml, "radiobutton4");
	g_return_val_if_fail (calendar->five != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->five),
				      (global_preferences->days == E_SUMMARY_CALENDAR_FIVE_DAYS));
	g_signal_connect (calendar->five, "toggled", G_CALLBACK (calendar_five_toggled_cb), pd);

	calendar->week = glade_xml_get_widget (pd->xml, "radiobutton5");
	g_return_val_if_fail (calendar->week != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->week),
				      (global_preferences->days == E_SUMMARY_CALENDAR_ONE_WEEK));
	g_signal_connect (calendar->week, "toggled", G_CALLBACK (calendar_week_toggled_cb), pd);

	calendar->month = glade_xml_get_widget (pd->xml, "radiobutton6");
	g_return_val_if_fail (calendar->month != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->month),
				      (global_preferences->days == E_SUMMARY_CALENDAR_ONE_MONTH));
	g_signal_connect (calendar->month, "toggled", G_CALLBACK (calendar_month_toggled_cb), pd);

	calendar->all = glade_xml_get_widget (pd->xml, "radiobutton1");
	g_return_val_if_fail (calendar->all != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->all),
				      (global_preferences->show_tasks == E_SUMMARY_CALENDAR_ALL_TASKS));
	g_signal_connect (calendar->all, "toggled", G_CALLBACK (calendar_all_toggled_cb), pd);

	calendar->today = glade_xml_get_widget (pd->xml, "radiobutton2");
	g_return_val_if_fail (calendar->today != NULL, FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (calendar->today),
				      (global_preferences->show_tasks == E_SUMMARY_CALENDAR_TODAYS_TASKS));
	g_signal_connect (calendar->today, "toggled", G_CALLBACK (calendar_today_toggled_cb), pd);

	return TRUE;
}

static void
free_property_dialog (PropertyData *pd)
{
	if (pd->rdf) {
		g_slist_free (pd->rdf->known);

		free_str_list (pd->rdf->tmp_list);
		g_slist_free (pd->rdf->tmp_list);
		g_free (pd->rdf);
	}
	if (pd->mail) {
		free_str_list (pd->mail->tmp_list);
		g_slist_free (pd->mail->tmp_list);
		g_free (pd->mail);
	}
	if (pd->weather) {
		free_str_list (pd->weather->tmp_list);
		g_slist_free (pd->weather->tmp_list);
		g_free (pd->weather);
	}
	if (pd->calendar) {
		g_free (pd->calendar);
	}

	if (pd->xml)
		g_object_unref (pd->xml);

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
	GSList *l;
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
	g_object_set_data (G_OBJECT (widget), "listener", listener);
	g_object_set_data (G_OBJECT (widget), "corba_view", view);

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
	GSList **list;

	item = (ESummaryShownModelEntry *) value;
	list = (GSList **) data;

	*list = g_slist_prepend (*list, g_strdup (item->location));
}

static GSList *
get_folders_from_view (GtkWidget *view)
{
	GNOME_Evolution_StorageSetView set_view;
	GNOME_Evolution_FolderList *list;
	CORBA_Environment ev;
	GSList *out_list = NULL;
	int i;
	
	set_view = g_object_get_data (G_OBJECT (view), "corba_view");
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
		out_list = g_slist_append (out_list, f);
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
		g_slist_free (pd->rdf->tmp_list);
		pd->rdf->tmp_list = NULL;
	}
	/* Take each news feed which is on and add it
	   to the shown list */
	g_hash_table_foreach (E_SUMMARY_SHOWN (pd->rdf->etable)->shown_model,
			      add_shown_to_list, &pd->rdf->tmp_list);
	
	if (global_preferences->rdf_urls) {
		free_str_list (global_preferences->rdf_urls);
		g_slist_free (global_preferences->rdf_urls);
	}
	
	global_preferences->rdf_urls = copy_str_list (pd->rdf->tmp_list);
	
	/* Weather */
	if (pd->weather->tmp_list) {
		free_str_list (pd->weather->tmp_list);
		g_slist_free (pd->weather->tmp_list);
		pd->weather->tmp_list = NULL;
	}
	
	g_hash_table_foreach (E_SUMMARY_SHOWN (pd->weather->etable)->shown_model,
			      add_shown_to_list, &pd->weather->tmp_list);
	if (global_preferences->stations) {
		free_str_list (global_preferences->stations);
		g_slist_free (global_preferences->stations);
	}
	global_preferences->stations = copy_str_list (pd->weather->tmp_list);
	
	/* Folders */
	if (pd->mail->tmp_list) {
		free_str_list (pd->mail->tmp_list);
		g_slist_free (pd->mail->tmp_list);
		pd->mail->tmp_list = NULL;
	}
	
	if (global_preferences->display_folders) {
		free_folder_list (global_preferences->display_folders);
		g_slist_free (global_preferences->display_folders);
	}
	global_preferences->display_folders = get_folders_from_view(pd->mail->storage_set_view);

	e_summary_preferences_save (global_preferences);

  	e_summary_reconfigure_all ();
}

static void
config_control_destroy_cb (EvolutionConfigControl *config_control, void *data)
{
	e_summary_preferences_restore(global_preferences);
	free_property_dialog ((PropertyData *)data);
}

BonoboObject *
e_summary_preferences_create_control (void)
{
	PropertyData *pd;
	GtkWidget *widget;

	pd = g_new0 (PropertyData, 1);

	pd->xml = glade_xml_new (EVOLUTION_GLADEDIR "/my-evolution.glade", NULL, NULL);
	g_return_val_if_fail (pd->xml != NULL, NULL);

	widget = glade_xml_get_widget (pd->xml, "notebook");
	if (widget == NULL || ! make_property_dialog (pd)) {
		g_warning ("Missing some part of XML file");
		free_property_dialog (pd);
		return NULL;
	}

	gtk_widget_ref (widget);
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);

	gtk_widget_show_all (widget);
	pd->config_control = evolution_config_control_new (widget);

	gtk_widget_unref (widget);

	g_signal_connect (pd->config_control, "apply", G_CALLBACK (config_control_apply_cb), pd);
	g_signal_connect (pd->config_control, "destroy", G_CALLBACK (config_control_destroy_cb), pd);

	return BONOBO_OBJECT (pd->config_control);
}

/* FIXME this kinda sucks.  */
void
e_summary_preferences_init_control (GNOME_Evolution_Shell corba_shell)
{
	global_shell = corba_shell;
}
