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
#include "e-util/e-icon-factory.h"
#include <calendar/gui/e-cal-config.h>
#include <calendar/gui/e-cal-event.h>
#include <calendar/gui/calendar-component.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-url.h>
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
void       e_calendar_weather_migrate (EPlugin *epl, ECalEventTargetComponent *data);
gint        e_plugin_lib_enable (EPluginLib *epl, gint enable);

#define WEATHER_BASE_URI "weather://"

gint
e_plugin_lib_enable (EPluginLib *epl, gint enable)
{
	GList *l;
	gboolean found = FALSE;
	const gchar *tmp;

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

	tmp = _(categories [0].description);

	/* Add the categories icons if we don't have them. */
	for (l = e_categories_get_list (); l; l = g_list_next (l)) {
		if (!strcmp ((const gchar *)l->data, tmp)) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		gint i;

		for (i = 0; categories[i].description; i++) {
			gchar *filename;

			filename = e_icon_factory_get_icon_filename (categories[i].icon_name, GTK_ICON_SIZE_MENU);
			e_categories_add (_(categories[i].description), NULL, filename, FALSE);
			g_free (filename);
		}
	}

	return 0;
}

void
e_calendar_weather_migrate (EPlugin *epl, ECalEventTargetComponent *data)
{
	/* Perform a migration step here. This allows us to keep the weather calendar completely
	 * separate from evolution. If the plugin isn't built, the weather source group won't
	 * show up in the user's evolution. If it is, this will create it if it doesn't exist */
	CalendarComponent *component;
	ESourceList *source_list;
	ESourceGroup *group;
	GSList *groups;
	ESourceGroup *weather = NULL;

	component = data->component;
	source_list = calendar_component_peek_source_list (component);

	groups = e_source_list_peek_groups (source_list);
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
		e_source_list_add_group (source_list, group, -1);

		weather = group;
	}

	if (weather)
		g_object_unref (weather);

	e_source_list_sync (source_list, NULL);
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
	GtkCellRenderer *text;
	GtkTreeSelection *selection;
	gchar *uri_text;
	EUri *uri;

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
	uri = e_uri_new (uri_text);
	if (uri->path && strlen (uri->path)) {
		GtkTreeIter *iter = find_location (store, uri_text + 10);
		GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter);
		gtk_tree_view_expand_to_path (GTK_TREE_VIEW (treeview), path);
		gtk_tree_selection_select_path (selection, path);
		gtk_tree_path_free (path);
	}
	g_free (uri_text);
	e_uri_free (uri);

	g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (selection_changed), dialog);
	g_object_set_data (G_OBJECT (dialog), "treeview", treeview);

	text = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), -1, "location", text, "text", 0, NULL);

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), scrolledwindow);
	gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), 6);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 6);

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
		if (strcmp ((const gchar *)text, _("None")) == 0)
			e_source_set_relative_uri (source, "");
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

GtkWidget *
e_calendar_weather_location (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	static GtkWidget *label;
	GtkWidget *button, *parent, *text;
	gint row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	EUri *uri;
	gchar *uri_text;
	static GtkWidget *hidden;

	if (store == NULL)
		store = gweather_xml_load_locations ();

	if (!hidden)
		hidden = gtk_label_new ("");

	if (data->old)
		gtk_widget_destroy (label);

	uri_text = e_source_get_uri (t->source);
	uri = e_uri_new (uri_text);
	if (strcmp ((const gchar *)uri->protocol, "weather")) {
		e_uri_free (uri);
		return hidden;
	}

	parent = data->parent;

	row = ((GtkTable*)parent)->nrows;

	label = gtk_label_new_with_mnemonic (_("_Location:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

	button = gtk_button_new ();
	g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (location_clicked), source);
	gtk_widget_show (button);

	if (uri->path && strlen (uri->path)) {
		GtkTreeIter *iter = find_location (store, uri_text + 10);
		gchar *location = build_location_path (iter);
		text = gtk_label_new (location);
		g_free (location);
	} else
		text = gtk_label_new (_("None"));
	gtk_widget_show (text);
	gtk_label_set_ellipsize (GTK_LABEL (text), PANGO_ELLIPSIZE_START);
	gtk_container_add (GTK_CONTAINER (button), text);
	e_uri_free (uri);
	g_free (uri_text);

	gtk_table_attach (GTK_TABLE (parent), button, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	return button;
}

static void
set_refresh_time (ESource *source, GtkWidget *spin, GtkWidget *combobox)
{
	gint time;
	gint item_num = 0;
	const gchar *refresh_str = e_source_get_property (source, "refresh");
	time = refresh_str ? atoi (refresh_str) : 30;

	if (time && !(time % 10080)) {
		/* weeks */
		item_num = 3;
		time /= 10080;
	} else if (time && !(time % 1440)) {
		/* days */
		item_num = 2;
		time /= 1440;
	} else if (time && !(time % 60)) {
		/* hours */
		item_num = 1;
		time /= 60;
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), item_num);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), time);
}

static gchar *
get_refresh_minutes (GtkWidget *spin, GtkWidget *combobox)
{
	gint setting = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
	switch (gtk_combo_box_get_active (GTK_COMBO_BOX (combobox))) {
	case 0:
		/* minutes */
		break;
	case 1:
		/* hours */
		setting *= 60;
		break;
	case 2:
		/* days */
		setting *= 1440;
		break;
	case 3:
		/* weeks - is this *really* necessary? */
		setting *= 10080;
		break;
	default:
		g_warning ("Time unit out of range");
		break;
	}
	return g_strdup_printf ("%d", setting);
}

static void
spin_changed (GtkSpinButton *spin, ECalConfigTargetSource *t)
{
	gchar *refresh_str;
	GtkWidget *combobox;

	combobox = g_object_get_data (G_OBJECT (spin), "combobox");

	refresh_str = get_refresh_minutes ((GtkWidget *) spin, combobox);
	e_source_set_property (t->source, "refresh", refresh_str);
	g_free (refresh_str);
}

static void
combobox_changed (GtkComboBox *combobox, ECalConfigTargetSource *t)
{
	gchar *refresh_str;
	GtkWidget *spin;

	spin = g_object_get_data (G_OBJECT (combobox), "spin");

	refresh_str = get_refresh_minutes (spin, (GtkWidget *) combobox);
	e_source_set_property (t->source, "refresh", refresh_str);
	g_free (refresh_str);
}

GtkWidget *
e_calendar_weather_refresh (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	static GtkWidget *label;
	GtkWidget *spin, *combobox, *hbox, *parent;
	gint row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	EUri *uri;
	gchar *uri_text;
	static GtkWidget *hidden = NULL;

	if (!hidden)
		hidden = gtk_label_new ("");

	if (data->old)
		gtk_widget_destroy (label);

	uri_text = e_source_get_uri (t->source);
	uri = e_uri_new (uri_text);
	g_free (uri_text);
	if (strcmp ((const gchar *)uri->protocol, "weather")) {
		e_uri_free (uri);
		return hidden;
	}
	e_uri_free (uri);

	parent = data->parent;

	row = ((GtkTable*)parent)->nrows;

	label = gtk_label_new_with_mnemonic (_("Re_fresh:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);

	spin = gtk_spin_button_new_with_range (0, 100, 1);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin);
	gtk_widget_show (spin);
	gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, TRUE, 0);

	combobox = gtk_combo_box_new_text ();
	gtk_widget_show (combobox);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _("minutes"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _("hours"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _("days"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combobox), _("weeks"));
	set_refresh_time (source, spin, combobox);
	gtk_box_pack_start (GTK_BOX (hbox), combobox, FALSE, TRUE, 0);

	g_object_set_data (G_OBJECT (combobox), "spin", spin);
	g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (combobox_changed), t);
	g_object_set_data (G_OBJECT (spin), "combobox", combobox);
	g_signal_connect (G_OBJECT (spin), "value-changed", G_CALLBACK (spin_changed), t);

	gtk_table_attach (GTK_TABLE (parent), hbox, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	return hbox;
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
	static GtkWidget *label;
	GtkWidget *combobox, *parent;
	gint row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	EUri *uri;
	gchar *uri_text;
	static GtkWidget *hidden = NULL;

	if (!hidden)
		hidden = gtk_label_new ("");

	if (data->old)
		gtk_widget_destroy (label);

	uri_text = e_source_get_uri (t->source);
	uri = e_uri_new (uri_text);
	g_free (uri_text);
	if (strcmp ((const gchar *)uri->protocol, "weather")) {
		e_uri_free (uri);
		return hidden;
	}
	e_uri_free (uri);

	parent = data->parent;

	row = ((GtkTable*)parent)->nrows;

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
	EUri *uri;
	gboolean ok = FALSE;
	ESourceGroup *group = e_source_peek_group (t->source);

	/* always return TRUE if this isn't a weather source */
	if (strncmp (e_source_group_peek_base_uri (group), "weather", 7))
		return TRUE;

	uri = e_uri_new (e_source_get_uri (t->source));
	/* make sure that the protocol is weather:// and that the path isn't empty */
	ok = (uri->path && strlen (uri->path));
	e_uri_free (uri);

	return ok;
}
