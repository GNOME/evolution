/*
 * Authors: David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 2005 Novell, Inc. (www.novell.com)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <gtk/gtk.h>
#include <e-util/e-config.h>
#include <calendar/gui/e-cal-config.h>
#include <calendar/gui/e-cal-event.h>
#include <calendar/gui/calendar-component.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-categories.h>
#include <libgnome/gnome-i18n.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>

GtkWidget *e_calendar_weather_location (EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *e_calendar_weather_refresh (EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *e_calendar_weather_units (EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean   e_calendar_weather_check (EPlugin *epl, EConfigHookPageCheckData *data);
void       e_calendar_weather_migrate (EPlugin *epl, ECalEventTargetComponent *data);
int        e_plugin_lib_enable (EPluginLib *epl, int enable);

static GtkTreeStore *store = NULL;

#define WEATHER_BASE_URI "weather://"

int
e_plugin_lib_enable (EPluginLib *epl, int enable)
{
	GList *l;
	gboolean found = FALSE;

	/* Add the categories icons if we don't have them. */
	for (l = e_categories_get_list (); l; l = g_list_next (l)) {
		if (!strcmp (l->data, _("Weather: Cloudy"))) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		e_categories_add (_("Weather: Cloudy"), NULL, WEATHER_DATADIR "/category_weather_cloudy_16.png");
		e_categories_add (_("Weather: Fog"), NULL, WEATHER_DATADIR "/category_weather_fog_16.png");
		e_categories_add (_("Weather: Partly Cloudy"), NULL, WEATHER_DATADIR "/category_weather_partly_cloudy_16.png");
		e_categories_add (_("Weather: Rain"), NULL, WEATHER_DATADIR "/category_weather_rain_16.png");
		e_categories_add (_("Weather: Snow"), NULL, WEATHER_DATADIR "/category_weather_snow_16.png");
		e_categories_add (_("Weather: Sunny"), NULL, WEATHER_DATADIR "/category_weather_sun_16.png");
		e_categories_add (_("Weather: Thunderstorms"), NULL, WEATHER_DATADIR "/category_weather_tstorm_16.png");
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
	return 0;
}

static void
parse_subtree (GtkTreeIter *parent, xmlNode *node)
{
	GtkTreeIter iter;
	xmlNode *child;

	if (node->type == XML_ELEMENT_NODE) {
		gtk_tree_store_append (store, &iter, parent);
		if (strcmp (node->name, "location") == 0) {
			xmlAttr *attr;

			child = node->children;
			g_assert (child->type == XML_TEXT_NODE);
			gtk_tree_store_set (store, &iter, 0, child->content, -1);

			for (attr = node->properties; attr; attr = attr->next) {
				if (strcmp (attr->name, "code") == 0)
					gtk_tree_store_set (store, &iter, 1, attr->children->content, -1);
				else if (strcmp (attr->name, "url") == 0)
					gtk_tree_store_set (store, &iter, 2, attr->children->content, -1);
				else if (strcmp (attr->name, "type") == 0)
					gtk_tree_store_set (store, &iter, 3, attr->children->content, -1);
			}
		} else {
			xmlAttr *attr;

			for (child = node->children; child; child = child->next)
				parse_subtree (&iter, child);

			for (attr = node->properties; attr; attr = attr->next)
				if (strcmp (attr->name, "name") == 0)
					gtk_tree_store_set (store, &iter, 0, attr->children->content, -1);
		}
	}
}

static void
load_locations ()
{
	xmlDoc *doc;
	xmlNode *root, *child;

	LIBXML_TEST_VERSION

	doc = xmlParseFile (WEATHER_EDS_DATADIR "/Locations.xml");
	if (doc == NULL) {
		g_warning ("failed to read locations file");
		return;
	}

	if (store == NULL)
		store = gtk_tree_store_new (4,
			G_TYPE_STRING,	/* name */
			G_TYPE_STRING,	/* code */
			G_TYPE_STRING,	/* URL  */
			G_TYPE_STRING);	/* type */

	root = xmlDocGetRootElement (doc);
	for (child = root->children; child; child = child->next)
		parse_subtree (NULL, child);
	xmlFreeDoc (doc);
}

static void
selection_changed (GtkTreeSelection *selection, GtkDialog *dialog)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gchar *code = NULL;
		gtk_tree_model_get (model, &iter, 1, &code, -1);
		if (code != NULL) {
			gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
		} else {
			GtkTreeView *view = gtk_tree_selection_get_tree_view (selection);
			GtkTreePath *path;
			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_view_expand_row (view, path, FALSE);
			gtk_tree_path_free (path);
			gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
		}
	} else {
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
	}
}

static struct
{
	gchar **ids;
	GtkTreeIter *result;
} find_data;

static gboolean
find_location_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *node, gpointer data)
{
	gchar *type, *code, *name;
	gtk_tree_model_get (model, node, 0, &name, 1, &code, 3, &type, -1);
	if (name == NULL || code == NULL || type == NULL)
		return FALSE;
	if ((!strcmp (type, find_data.ids[0])) &&
	    (!strcmp (code, find_data.ids[1])) &&
	    (!strcmp (name, find_data.ids[2]))) {
		find_data.result = gtk_tree_iter_copy (node);
		return TRUE;
	}
	return FALSE;
}

static GtkTreeIter *
find_location (gchar *relative_url)
{
	/* type/code/name */
	find_data.ids = g_strsplit (relative_url, "/", -1);
	find_data.result = NULL;
	gtk_tree_model_foreach (GTK_TREE_MODEL (store), (GtkTreeModelForeachFunc) find_location_func, NULL);

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
			gchar *code = NULL;
			gtk_tree_model_get (model, &iter, 1, &code, -1);
			if (code != NULL) {
				gtk_dialog_response (dialog, GTK_RESPONSE_OK);
				return TRUE;
			}
		}
	}
	return FALSE;
}

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
	treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
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
		GtkTreeIter *iter = find_location (uri_text + 10);
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

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter, 0, &temp1, -1);
	path = g_strdup (temp1);

	while (gtk_tree_model_iter_parent (GTK_TREE_MODEL (store), &parent, iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (store), &parent, 0, &temp1, -1);
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
		gchar *type, *code, *name;
		gchar *path, *uri;

		gtk_tree_selection_get_selected (selection, &model, &iter);
		gtk_tree_model_get (model, &iter, 0, &name, 1, &code, 3, &type, -1);
		path = build_location_path (&iter);

		label = gtk_bin_get_child (GTK_BIN (button));
		gtk_label_set_text (GTK_LABEL (label), path);

		uri = g_strdup_printf ("%s/%s/%s", type, code, name);
		/* FIXME - url_encode (&uri); */
		e_source_set_relative_uri (source, uri);
		g_free (uri);
	} else {
		GtkWidget *label;
		const gchar *text;

		label = GTK_WIDGET (gtk_bin_get_child (GTK_BIN (button)));
		text = gtk_label_get_text (GTK_LABEL (label));
		if (strcmp (text, _("None")) == 0)
			e_source_set_relative_uri (source, "");
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

GtkWidget *
e_calendar_weather_location (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	static GtkWidget *label;
	GtkWidget *button, *parent, *text;
	int row;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	EUri *uri;
	char *uri_text;
	static GtkWidget *hidden;

	if (store == NULL)
		load_locations ();

	if (!hidden)
		hidden = gtk_label_new ("");

	if (data->old)
		gtk_widget_destroy (label);

	uri_text = e_source_get_uri (t->source);
	uri = e_uri_new (uri_text);
	if (strcmp (uri->protocol, "weather")) {
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
		GtkTreeIter *iter = find_location (uri_text + 10);
		gchar *location = build_location_path (iter);
		text = gtk_label_new (location);
		g_free (location);
	} else
		text = gtk_label_new (_("None"));
	gtk_widget_show (text);
#if (GTK_CHECK_VERSION(2, 6, 0))
	gtk_label_set_ellipsize (GTK_LABEL (text), PANGO_ELLIPSIZE_START);
#endif
	gtk_container_add (GTK_CONTAINER (button), text);
	e_uri_free (uri);
	g_free (uri_text);

	gtk_table_attach (GTK_TABLE (parent), button, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	return button;
}

static void
set_refresh_time (ESource *source, GtkWidget *spin, GtkWidget *option)
{
	int time;
	int item_num = 0;
	const char *refresh_str = e_source_get_property (source, "refresh");
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
	gtk_option_menu_set_history (GTK_OPTION_MENU (option), item_num);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), time);
}

static char *
get_refresh_minutes (GtkWidget *spin, GtkWidget *option)
{
	int setting = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
	switch (gtk_option_menu_get_history (GTK_OPTION_MENU (option))) {
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
	char *refresh_str;
	GtkWidget *option;

	option = g_object_get_data (G_OBJECT (spin), "option");

	refresh_str = get_refresh_minutes ((GtkWidget *) spin, option);
	e_source_set_property (t->source, "refresh", refresh_str);
	g_free (refresh_str);
}

static void
option_changed (GtkOptionMenu *option, ECalConfigTargetSource *t)
{
	char *refresh_str;
	GtkWidget *spin;

	spin = g_object_get_data (G_OBJECT (option), "spin");

	refresh_str = get_refresh_minutes (spin, (GtkWidget *) option);
	e_source_set_property (t->source, "refresh", refresh_str);
	g_free (refresh_str);
}

GtkWidget *
e_calendar_weather_refresh (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	static GtkWidget *label;
	GtkWidget *option, *spin, *menu, *hbox, *parent;
	GtkWidget *times[4];
	int row, i;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	EUri *uri;
	char *uri_text;
	static GtkWidget *hidden = NULL;

	if (!hidden)
		hidden = gtk_label_new ("");

	if (data->old)
		gtk_widget_destroy (label);

	uri_text = e_source_get_uri (t->source);
	uri = e_uri_new (uri_text);
	g_free (uri_text);
	if (strcmp (uri->protocol, "weather")) {
		e_uri_free (uri);
		return hidden;
	}
	e_uri_free (uri);

	parent = data->parent;

	row = ((GtkTable*)parent)->nrows;

	label = gtk_label_new_with_mnemonic (_("_Refresh:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (parent), label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);

	spin = gtk_spin_button_new_with_range (0, 100, 1);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin);
	gtk_widget_show (spin);
	gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, TRUE, 0);

	option = gtk_option_menu_new ();
	gtk_widget_show (option);
	times[0] = gtk_menu_item_new_with_label (_("minutes"));
	times[1] = gtk_menu_item_new_with_label (_("hours"));
	times[2] = gtk_menu_item_new_with_label (_("days"));
	times[3] = gtk_menu_item_new_with_label (_("weeks"));
	menu = gtk_menu_new ();
	gtk_widget_show (menu);
	for (i = 0; i < 4; i++) {
		gtk_widget_show (times[i]);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), times[i]);
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option), menu);
	set_refresh_time (source, spin, option);
	gtk_box_pack_start (GTK_BOX (hbox), option, FALSE, TRUE, 0);

	g_object_set_data (G_OBJECT (option), "spin", spin);
	g_signal_connect (G_OBJECT (option), "changed", G_CALLBACK (option_changed), t);
	g_object_set_data (G_OBJECT (spin), "option", option);
	g_signal_connect (G_OBJECT (spin), "value-changed", G_CALLBACK (spin_changed), t);

	gtk_table_attach (GTK_TABLE (parent), hbox, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	return hbox;
}

static void
set_units (ESource *source, GtkWidget *option)
{
	const char *format = e_source_get_property (source, "units");
	if (format == NULL) {
		format = e_source_get_property (source, "temperature");
		if (format == NULL) {
			e_source_set_property (source, "units", "metric");
			gtk_option_menu_set_history (GTK_OPTION_MENU (option), 0);
		} else if (strcmp (format, "fahrenheit") == 0) {
			/* old format, convert */
			e_source_set_property (source, "units", "imperial");
			gtk_option_menu_set_history (GTK_OPTION_MENU (option), 1);
		} else {
			e_source_set_property (source, "units", "metric");
			gtk_option_menu_set_history (GTK_OPTION_MENU (option), 0);
		}
	} else {
		if (strcmp (format, "metric") == 0)
			gtk_option_menu_set_history (GTK_OPTION_MENU (option), 0);
		else
			gtk_option_menu_set_history (GTK_OPTION_MENU (option), 1);
	}
}

static void
units_changed (GtkOptionMenu *option, ECalConfigTargetSource *t)
{
	int choice = gtk_option_menu_get_history (GTK_OPTION_MENU (option));
	if (choice == 0)
		e_source_set_property (t->source, "units", "metric");
	else
		e_source_set_property (t->source, "units", "imperial");
}

GtkWidget *
e_calendar_weather_units (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	static GtkWidget *label;
	GtkWidget *option, *menu, *parent;
	GtkWidget *formats[2];
	int row, i;
	ECalConfigTargetSource *t = (ECalConfigTargetSource *) data->target;
	ESource *source = t->source;
	EUri *uri;
	char *uri_text;
	static GtkWidget *hidden = NULL;

	if (!hidden)
		hidden = gtk_label_new ("");

	if (data->old)
		gtk_widget_destroy (label);

	uri_text = e_source_get_uri (t->source);
	uri = e_uri_new (uri_text);
	g_free (uri_text);
	if (strcmp (uri->protocol, "weather")) {
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

	option = gtk_option_menu_new ();
	gtk_widget_show (option);
	formats[0] = gtk_menu_item_new_with_label (_("Metric (celcius, cm, etc)"));
	formats[1] = gtk_menu_item_new_with_label (_("Imperial (fahrenheit, inches, etc)"));
	menu = gtk_menu_new ();
	gtk_widget_show (menu);
	for (i = 0; i < 2; i++) {
		gtk_widget_show (formats[i]);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), formats[i]);
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option), menu);
	set_units (source, option);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), option);
	g_signal_connect (G_OBJECT (option), "changed", G_CALLBACK (units_changed), t);
	gtk_table_attach (GTK_TABLE (parent), option, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);

	return option;
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
