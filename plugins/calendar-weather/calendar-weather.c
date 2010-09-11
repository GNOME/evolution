/*
 * Authors: David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <gtk/gtk.h>
#include <e-util/e-config.h>
#include <e-util/e-icon-factory.h>
#include <e-util/e-plugin-util.h>
#include <calendar/gui/e-cal-config.h>
#include <calendar/gui/e-cal-event.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-categories.h>
#include <glib/gi18n.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/weather.h>
#include <libgweather/gweather-xml.h>
#undef GWEATHER_I_KNOW_THIS_IS_UNSTABLE

GtkWidget *e_calendar_weather_location (EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *e_calendar_weather_refresh (EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *e_calendar_weather_units (EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean   e_calendar_weather_check (EPlugin *epl, EConfigHookPageCheckData *data);
void       e_calendar_weather_migrate (EPlugin *epl, ECalEventTargetBackend *data);
gint        e_plugin_lib_enable (EPlugin *epl, gint enable);

#define WEATHER_BASE_URI "weather://"

gint
e_plugin_lib_enable (EPlugin *epl, gint enable)
{
	GList *l;
	const gchar *tmp;
	gint ii;

	static struct {
		const gchar *description;
		const gchar *icon_name;
	} categories[] = {
		{ N_("Weather: Fog"),		"weather-fog" },
		{ N_("Weather: Cloudy"),	"weather-few-clouds" },
		{ N_("Weather: Cloudy Night"),	"weather-few-clouds-night" },
		{ N_("Weather: Overcast"),	"weather-overcast" },
		{ N_("Weather: Showers"),	"weather-showers" },
		{ N_("Weather: Snow"),		"weather-snow" },
		{ N_("Weather: Sunny"),	"weather-clear" },
		{ N_("Weather: Clear Night"),	"weather-clear-night" },
		{ N_("Weather: Thunderstorms"), "weather-storm" },
		{ NULL,				NULL }
	};

	tmp = _(categories[0].description);

	/* Add the categories icons if we don't have them. */
	for (l = e_categories_get_list (); l; l = g_list_next (l)) {
		if (!strcmp ((const gchar *)l->data, tmp))
			goto exit;
	}

	for (ii = 0; categories[ii].description; ii++) {
		gchar *filename;

		filename = e_icon_factory_get_icon_filename (
			categories[ii].icon_name, GTK_ICON_SIZE_MENU);
		e_categories_add (
			_(categories[ii].description), NULL, filename, FALSE);
		g_free (filename);
	}

exit:
	return 0;
}

void
e_calendar_weather_migrate (EPlugin *epl, ECalEventTargetBackend *data)
{
	/* Perform a migration step here. This allows us to keep the weather calendar completely
	 * separate from evolution. If the plugin isn't built, the weather source group won't
	 * show up in the user's evolution. If it is, this will create it if it doesn't exist */
	ESourceGroup *group;
	GSList *groups;
	ESourceGroup *weather = NULL;

	groups = e_source_list_peek_groups (data->source_list);
	if (groups) {
		/* groups are already there, we need to search */
		GSList *g;

		for (g = groups; g; g = g_slist_next (g)) {
			group = E_SOURCE_GROUP (g->data);
			if (!weather && !strcmp (WEATHER_BASE_URI, e_source_group_peek_base_uri (group)))
				weather = g_object_ref (group);
		}
	}

	if (!weather) {
		group = e_source_group_new (_("Weather"), WEATHER_BASE_URI);
		e_source_list_add_group (data->source_list, group, -1);

		weather = group;
	}

	if (weather)
		g_object_unref (weather);

	e_source_list_sync (data->source_list, NULL);
}

static void
selection_changed (GtkTreeSelection *selection, GtkDialog *dialog)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		WeatherLocation *loc = NULL;
		gtk_tree_model_get (model, &iter, GWEATHER_XML_COL_POINTER, &loc, -1);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, loc != NULL);
	} else {
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
	}
}

static struct
{
	gboolean is_old;
	gchar **ids;
	GtkTreeIter *result;
} find_data;

static gboolean
find_location_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *node, gpointer data)
{
	WeatherLocation *wl = NULL;

	gtk_tree_model_get (model, node, GWEATHER_XML_COL_POINTER, &wl, -1);
	if (!wl || !wl->name || !wl->code)
		return FALSE;
	if (((!strcmp (wl->code, find_data.ids[0])) || (find_data.is_old && !strcmp (wl->code + 1, find_data.ids[0]))) &&
	     (!strcmp (wl->name, find_data.ids[1]))) {
		find_data.result = gtk_tree_iter_copy (node);
		return TRUE;
	}
	return FALSE;
}

static GtkTreeIter *
find_location (GtkTreeModel *model, gchar *relative_url)
{
	/* old URL uses type/code/name, but new uses only code/name */
	if (strncmp (relative_url, "ccf/", 4) == 0) {
		relative_url = relative_url + 4;
		find_data.is_old = TRUE;
	} else
		find_data.is_old = FALSE;

	find_data.ids = g_strsplit (relative_url, "/", -1);
	find_data.result = NULL;
	gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc) find_location_func, NULL);

	g_strfreev (find_data.ids);
	return find_data.result;
}

static gboolean
treeview_clicked (GtkTreeView *treeview, GdkEventButton *event, GtkDialog *dialog)
{
	if (event->type == GDK_2BUTTON_PRESS) {
		GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
		GtkTreeModel *model;
		GtkTreeIter iter;

		if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
			WeatherLocation *wl = NULL;
			gtk_tree_model_get (model, &iter, GWEATHER_XML_COL_POINTER, &wl, -1);
			if (wl != NULL && wl->code != NULL && wl->name != NULL) {
				gtk_dialog_response (dialog, GTK_RESPONSE_OK);
				return TRUE;
			}
		}
	}
	return FALSE;
}

static GtkTreeModel *store = NULL;

static GtkDialog *
create_source_selector (ESource *source)
{
	GtkWidget *dialog, *treeview, *scrolledwindow;
	GtkWidget *content_area;
	GtkCellRenderer *text;
	GtkTreeSelection *selection;
	gchar *uri_text;
	SoupURI *suri;

	/* FIXME - should show an error here if it fails*/
	if (store == NULL)
		return NULL;

	dialog = gtk_dialog_new_with_buttons (
		_("Select a location"),
		NULL, GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scrolledwindow);
	treeview = gtk_tree_view_new_with_model (store);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
	gtk_widget_show (treeview);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolledwindow), treeview);
	gtk_widget_add_events (treeview, GDK_BUTTON_PRESS);
	g_signal_connect (G_OBJECT (treeview), "button-press-event", G_CALLBACK (treeview_clicked), dialog);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

	uri_text = e_source_get_uri (source);
	suri = soup_uri_new (uri_text);
	if (suri && suri->path && *suri->path) {
		GtkTreeIter *iter = find_location (store, uri_text + 10);
		GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter);
		gtk_tree_view_expand_to_path (GTK_TREE_VIEW (treeview), path);
		gtk_tree_selection_select_path (selection, path);
		gtk_tree_path_free (path);
	}
	g_free (uri_text);
	if (suri)
		soup_uri_free (suri);

	g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (selection_changed), dialog);
	g_object_set_data (G_OBJECT (dialog), "treeview", treeview);

	text = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1, "location", text, "text", 0, NULL);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_add (GTK_CONTAINER (content_area), scrolledwindow);
	gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), 6);
	gtk_box_set_spacing (GTK_BOX (content_area), 6);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 420, 340);

	return GTK_DIALOG (dialog);
}

static gchar *
build_location_path (GtkTreeIter *iter)
{
	GtkTreeIter parent;
	gchar *path, *temp1, *temp2;

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter, GWEATHER_XML_COL_LOC, &temp1, -1);
	path = g_strdup (temp1);

	while (gtk_tree_model_iter_parent (GTK_TREE_MODEL (store), &parent, iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (store), &parent, GWEATHER_XML_COL_LOC, &temp1, -1);
		temp2 = g_strdup_printf ("%s : %s", temp1, path);
		g_free (path);
		path = temp2;
		iter = gtk_tree_iter_copy (&parent);
	}
	return path;
}

static void
location_clicked (GtkButton *button, ESource *source)
{
	GtkDialog *dialog = create_source_selector (source);
	gint response;

	if (dialog == NULL)
		return;

	response = gtk_dialog_run (dialog);

	if (response == GTK_RESPONSE_OK) {
		GtkTreeView *view = GTK_TREE_VIEW (g_object_get_data (G_OBJECT (dialog), "treeview"));
		GtkTreeSelection *selection = gtk_tree_view_get_selection (view);
		GtkTreeModel *model;
		GtkTreeIter iter;
		GtkWidget *label;
		WeatherLocation *wl = NULL;
		gchar *path, *uri;

		gtk_tree_selection_get_selected (selection, &model, &iter);
		gtk_tree_model_get (model, &iter, GWEATHER_XML_COL_POINTER, &wl, -1);
		path = build_location_path (&iter);

		label = gtk_bin_get_child (GTK_BIN (button));
		gtk_label_set_text (GTK_LABEL (label), path);

		uri = g_strdup_printf ("%s/%s", wl->code, wl->name);
		/* FIXME - url_encode (&uri); */
		e_source_set_relative_uri (source, uri);
		g_free (uri);
	} else {
		GtkWidget *label;
		const gchar *text;

		label = GTK_WIDGET (gtk_bin_get_child (GTK_BIN (button)));
		text = gtk_label_get_text (GTK_LABEL (label));
		/* Translators: "None" location for a weather calendar */
		if (strcmp ((const gchar *)text, C_("weather-cal-location", "None")) == 0)
			e_source_set_relative_uri (source, "");
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

GtkWidget *
e_calendar_weather_location (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	GtkWidget *button, *parent, *text, *label;
	guint row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	SoupURI *suri;
	gchar *uri_text;

	if (!e_plugin_util_is_source_proto (t->source, "weather"))
		return NULL;

	if (store == NULL)
		store = gweather_xml_load_locations ();

	uri_text = e_source_get_uri (t->source);
	suri = soup_uri_new (uri_text);

	parent = data->parent;

	g_object_get (parent, "n-rows", &row, NULL);

	label = gtk_label_new_with_mnemonic (_("_Location:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

	button = gtk_button_new ();
	g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (location_clicked), source);
	gtk_widget_show (button);

	if (suri && suri->path && *suri->path) {
		GtkTreeIter *iter = find_location (store, uri_text + 10);
		gchar *location = build_location_path (iter);
		text = gtk_label_new (location);
		g_free (location);
	} else {
		text = gtk_label_new (C_("weather-cal-location", "None"));
	}
	gtk_widget_show (text);
	gtk_label_set_ellipsize (GTK_LABEL (text), PANGO_ELLIPSIZE_START);
	gtk_container_add (GTK_CONTAINER (button), text);
	if (suri)
		soup_uri_free (suri);
	g_free (uri_text);

	gtk_table_attach (GTK_TABLE (parent), button, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	return button;
}

GtkWidget *
e_calendar_weather_refresh (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;

	if (!e_plugin_util_is_source_proto (t->source, "weather"))
		return NULL;

	return e_plugin_util_add_refresh (data->parent, _("Re_fresh:"), t->source, "refresh");
}

static void
set_units (ESource *source, GtkWidget *combobox)
{
	const gchar *format = e_source_get_property (source, "units");
	if (format == NULL) {
		format = e_source_get_property (source, "temperature");
		if (format == NULL) {
			e_source_set_property (source, "units", "metric");
			gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
		} else if (strcmp ((const gchar *)format, "fahrenheit") == 0) {
			/* old format, convert */
			e_source_set_property (source, "units", "imperial");
			gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 1);
		} else {
			e_source_set_property (source, "units", "metric");
			gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
		}
	} else {
		if (strcmp ((const gchar *)format, "metric") == 0)
			gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 0);
		else
			gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), 1);
	}
}

static void
units_changed (GtkComboBox *combobox, ECalConfigTargetSource *t)
{
	gint choice = gtk_combo_box_get_active (GTK_COMBO_BOX (combobox));
	if (choice == 0)
		e_source_set_property (t->source, "units", "metric");
	else
		e_source_set_property (t->source, "units", "imperial");
}

GtkWidget *
e_calendar_weather_units (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	GtkWidget *combobox, *parent, *label;
	guint row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;

	if (!e_plugin_util_is_source_proto (t->source, "weather"))
		return NULL;

	parent = data->parent;

	g_object_get (parent, "n-rows", &row, NULL);

	label = gtk_label_new_with_mnemonic (_("_Units:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

	combobox = gtk_combo_box_new_text ();
	gtk_widget_show (combobox);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _("Metric (Celsius, cm, etc)"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _("Imperial (Fahrenheit, inches, etc)"));
	set_units (source, combobox);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), combobox);
	g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (units_changed), t);
	gtk_table_attach (GTK_TABLE (parent), combobox, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);

	return combobox;
}

gboolean
e_calendar_weather_check (EPlugin *epl, EConfigHookPageCheckData *data)
{
	/* FIXME - check pageid */
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	SoupURI *suri;
	gboolean ok = FALSE;

	/* always return TRUE if this isn't a weather source */
	if (!e_plugin_util_is_group_proto (e_source_peek_group (t->source), "weather"))
		return TRUE;

	suri = soup_uri_new (e_source_get_uri (t->source));
	/* make sure that the protocol is weather:// and that the path isn't empty */
	ok = suri && suri->path && *suri->path;
	if (suri)
		soup_uri_free (suri);

	return ok;
}
