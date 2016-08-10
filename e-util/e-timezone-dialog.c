/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-timezone-dialog.h"

#include <time.h>
#include <string.h>
#include <glib/gi18n.h>

#include <libecal/libecal.h>

#include "e-map.h"
#include "e-misc-utils.h"
#include "e-util-private.h"

#ifdef G_OS_WIN32
#ifdef gmtime_r
#undef gmtime_r
#endif
#ifdef localtime_r
#undef localtime_r
#endif

/* The gmtime() and localtime() in Microsoft's C library are MT-safe */
#define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
#define localtime_r(tp,tmp) (localtime(tp)?(*(tmp)=*localtime(tp),(tmp)):0)
#endif

#define E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA 0xc070a0ff
#define E_TIMEZONE_DIALOG_MAP_POINT_HOVER_RGBA 0xffff60ff
#define E_TIMEZONE_DIALOG_MAP_POINT_SELECTED_1_RGBA 0xff60e0ff
#define E_TIMEZONE_DIALOG_MAP_POINT_SELECTED_2_RGBA 0x000000ff

#define E_TIMEZONE_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TIMEZONE_DIALOG, ETimezoneDialogPrivate))

struct _ETimezoneDialogPrivate {
	/* The selected timezone. May be NULL for a 'local time' (i.e. when
	 * the displayed name is ""). */
	icaltimezone *zone;

	/* In case a non-builtin timezone is used. */
	GSList *custom_zones; /* icaltimezone * */

	GtkBuilder *builder;

	EMapPoint *point_selected;
	EMapPoint *point_hover;

	EMap *map;
	GHashTable *index; /* const gchar *location ~> GtkTreeIter * */

	/* The timeout used to flash the nearest point. */
	guint timeout_id;

	/* Widgets from the UI file */
	GtkWidget *app;
	GtkWidget *table;
	GtkWidget *map_window;
	GtkWidget *timezone_combo;
	GtkWidget *preview_label;
};

static void e_timezone_dialog_dispose		(GObject	*object);

static gboolean get_widgets			(ETimezoneDialog *etd);
static gboolean on_map_timeout			(gpointer	 data);
static gboolean on_map_motion			(GtkWidget	*widget,
						 GdkEventMotion *event,
						 gpointer	 data);
static gboolean on_map_leave			(GtkWidget	*widget,
						 GdkEventCrossing *event,
						 gpointer	 data);
static gboolean on_map_visibility_changed	(GtkWidget	*w,
						 GdkEventVisibility *event,
						 gpointer	 data);
static gboolean on_map_button_pressed		(GtkWidget	*w,
						 GdkEvent	*button_event,
						 gpointer	 data);

static icaltimezone * get_zone_from_point	(ETimezoneDialog *etd,
						 EMapPoint	*point);
static void	set_map_timezone		(ETimezoneDialog *etd,
						 icaltimezone    *zone);
static void	on_combo_changed		(GtkComboBox	*combo,
						 ETimezoneDialog *etd);

static void timezone_combo_get_active_text	(GtkComboBox *combo,
						 gchar **zone_name);
static gboolean timezone_combo_set_active_text	(ETimezoneDialog *etd,
						 const gchar *zone_name);

static void	map_destroy_cb			(gpointer data,
						 GObject *where_object_was);

G_DEFINE_TYPE (ETimezoneDialog, e_timezone_dialog, G_TYPE_OBJECT)

/* Class initialization function for the event editor */
static void
e_timezone_dialog_class_init (ETimezoneDialogClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ETimezoneDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = e_timezone_dialog_dispose;
}

/* Object initialization function for the event editor */
static void
e_timezone_dialog_init (ETimezoneDialog *etd)
{
	etd->priv = E_TIMEZONE_DIALOG_GET_PRIVATE (etd);
	etd->priv->index = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
}

/* Dispose handler for the event editor */
static void
e_timezone_dialog_dispose (GObject *object)
{
	ETimezoneDialogPrivate *priv;

	priv = E_TIMEZONE_DIALOG_GET_PRIVATE (object);

	/* Destroy the actual dialog. */
	if (priv->app != NULL) {
		gtk_widget_destroy (priv->app);
		priv->app = NULL;
	}

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	if (priv->builder) {
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}

	if (priv->index) {
		g_hash_table_destroy (priv->index);
		priv->index = NULL;
	}

	g_slist_free (priv->custom_zones);
	priv->custom_zones = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_timezone_dialog_parent_class)->dispose (object);
}

static GtkTreeIter *
e_timezone_dialog_ensure_parent (GtkTreeStore *tree_store,
				 GHashTable *parents,
				 const gchar *location,
				 const gchar **name)
{
	GtkTreeIter *parent = NULL;
	const gchar *slash, *lookup_from;
	gchar *part;

	g_return_val_if_fail (GTK_IS_TREE_STORE (tree_store), NULL);
	g_return_val_if_fail (parents != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	if (!location)
		return NULL;

	*name = location;
	lookup_from = location;

	while (slash = strchr (lookup_from, '/'), slash) {
		GtkTreeIter *new_parent;
		gchar *full_part;

		*name = slash + 1;
		full_part = g_strndup (location, slash - location);
		part = g_strndup (lookup_from, slash - lookup_from);

		new_parent = g_hash_table_lookup (parents, full_part);
		if (!new_parent) {
			new_parent = g_new (GtkTreeIter, 1);

			gtk_tree_store_append (tree_store, new_parent, parent);
			gtk_tree_store_set (tree_store, new_parent, 0, part, -1);

			g_hash_table_insert (parents, full_part, new_parent);
		} else {
			g_free (full_part);
		}

		g_free (part);

		parent = new_parent;
		lookup_from = slash + 1;
	}

	return parent;
}

static void
e_timezone_dialog_add_timezones (ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;
	icalarray *zones;
	GtkComboBox *combo;
	GList *l, *list_items = NULL;
	GtkTreeStore *tree_store;
	GtkTreeIter iter;
	GtkCellRenderer *cell;
	GtkCssProvider *css_provider;
	GtkStyleContext *style_context;
	GHashTable *parents;
	const gchar *css;
	gint i;
	GError *error = NULL;

	priv = etd->priv;

	g_hash_table_remove_all (priv->index);

	/* Get the array of builtin timezones. */
	zones = icaltimezone_get_builtin_timezones ();

	for (i = 0; i < zones->num_elements; i++) {
		icaltimezone *zone;
		gchar *location;

		zone = icalarray_element_at (zones, i);

		location = _(icaltimezone_get_location (zone));

		e_map_add_point (
			priv->map, location,
			icaltimezone_get_longitude (zone),
			icaltimezone_get_latitude (zone),
			E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);

		list_items = g_list_prepend (list_items, location);
	}

	list_items = g_list_sort (list_items, (GCompareFunc) g_utf8_collate);

	/* Put the "UTC" entry at the top of the combo's list. */
	list_items = g_list_prepend (list_items, _("UTC"));

	combo = GTK_COMBO_BOX (priv->timezone_combo);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell, "text", 0, NULL);

	e_binding_bind_property (combo, "popup-shown", cell, "visible", G_BINDING_SYNC_CREATE);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell, "text", 1, NULL);

	e_binding_bind_property (combo, "popup-shown", cell, "visible", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	parents = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	tree_store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	for (l = list_items, i = 0; l != NULL; l = l->next, ++i) {
		GtkTreeIter *piter, *parent = NULL;
		const gchar *name = NULL;
		const gchar *location = l->data;

		parent = e_timezone_dialog_ensure_parent (tree_store, parents, location, &name);
		gtk_tree_store_append (tree_store, &iter, parent);
		gtk_tree_store_set (tree_store, &iter, 0, (gchar *) name, 1, (gchar *) location, -1);

		piter = g_new (GtkTreeIter, 1);
		*piter = iter;

		g_hash_table_insert (priv->index, (gchar *) location, piter);
	}

	g_hash_table_destroy (parents);

	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (tree_store));

	css_provider = gtk_css_provider_new ();
	css = "GtkComboBox { -GtkComboBox-appears-as-list: 1; }";
	gtk_css_provider_load_from_data (css_provider, css, -1, &error);
	style_context = gtk_widget_get_style_context (priv->timezone_combo);
	if (error == NULL) {
		gtk_style_context_add_provider (
			style_context,
			GTK_STYLE_PROVIDER (css_provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	} else {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	}
	g_object_unref (css_provider);

	g_list_free (list_items);
}

ETimezoneDialog *
e_timezone_dialog_construct (ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;
	GtkWidget *widget;
	GtkWidget *map;

	g_return_val_if_fail (etd != NULL, NULL);
	g_return_val_if_fail (E_IS_TIMEZONE_DIALOG (etd), NULL);

	priv = etd->priv;

	/* Load the content widgets */

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (priv->builder, "e-timezone-dialog.ui");

	if (!get_widgets (etd)) {
		g_message (
			"%s(): Could not find all widgets in the XML file!",
			G_STRFUNC);
		goto error;
	}

	widget = gtk_dialog_get_content_area (GTK_DIALOG (priv->app));
	gtk_container_set_border_width (GTK_CONTAINER (widget), 0);

	widget = gtk_dialog_get_action_area (GTK_DIALOG (priv->app));
	gtk_container_set_border_width (GTK_CONTAINER (widget), 12);

	priv->map = e_map_new ();
	map = GTK_WIDGET (priv->map);

	g_object_weak_ref (G_OBJECT (map), map_destroy_cb, priv);

	gtk_widget_set_events (
		map,
		gtk_widget_get_events (map) |
		GDK_LEAVE_NOTIFY_MASK |
		GDK_VISIBILITY_NOTIFY_MASK);

	e_timezone_dialog_add_timezones (etd);

	gtk_container_add (GTK_CONTAINER (priv->map_window), map);
	gtk_widget_show (map);

	/* Ensure a reasonable minimum amount of map is visible */
	gtk_widget_set_size_request (priv->map_window, 200, 200);

	g_signal_connect (
		map, "motion-notify-event",
		G_CALLBACK (on_map_motion), etd);
	g_signal_connect (
		map, "leave-notify-event",
		G_CALLBACK (on_map_leave), etd);
	g_signal_connect (
		map, "visibility-notify-event",
		G_CALLBACK (on_map_visibility_changed), etd);
	g_signal_connect (
		map, "button-press-event",
		G_CALLBACK (on_map_button_pressed), etd);

	g_signal_connect (
		priv->timezone_combo, "changed",
		G_CALLBACK (on_combo_changed), etd);

	return etd;

 error:

	g_object_unref (etd);
	return NULL;
}

#if 0
static gint
get_local_offset (void)
{
	time_t now = time (NULL), t_gmt, t_local;
	struct tm gmt, local;
	gint diff;

	gmtime_r (&now, &gmt);
	localtime_r (&now, &local);
	t_gmt = mktime (&gmt);
	t_local = mktime (&local);
	diff = t_local - t_gmt;

	return diff;
}
#endif

static icaltimezone *
get_local_timezone (void)
{
	icaltimezone *zone;
	gchar *location;

	tzset ();
	location = e_cal_system_timezone_get_location ();

	if (location)
		zone = icaltimezone_get_builtin_timezone (location);
	else
		zone = icaltimezone_get_utc_timezone ();

	g_free (location);

	return zone;
}

/* Gets the widgets from the XML file and returns if they are all available.
 * For the widgets whose values can be simply set with e-dialog-utils, it does
 * that as well.
 */
static gboolean
get_widgets (ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;
	GtkBuilder *builder;

	priv = etd->priv;
	builder = etd->priv->builder;

	priv->app = e_builder_get_widget (builder, "timezone-dialog");
	priv->map_window = e_builder_get_widget (builder, "map-window");
	priv->timezone_combo = e_builder_get_widget (builder, "timezone-combo");
	priv->table = e_builder_get_widget (builder, "timezone-table");
	priv->preview_label = e_builder_get_widget (builder, "preview-label");

	return (priv->app
		&& priv->map_window
		&& priv->timezone_combo
		&& priv->table
		&& priv->preview_label);
}

/**
 * e_timezone_dialog_new:
 *
 * Creates a new event editor dialog.
 *
 * Return value: A newly-created event editor dialog, or NULL if the event
 * editor could not be created.
 **/
ETimezoneDialog *
e_timezone_dialog_new (void)
{
	ETimezoneDialog *etd;

	etd = E_TIMEZONE_DIALOG (g_object_new (E_TYPE_TIMEZONE_DIALOG, NULL));
	return e_timezone_dialog_construct (E_TIMEZONE_DIALOG (etd));
}

static void
format_utc_offset (gint utc_offset,
                   gchar *buffer)
{
	const gchar *sign = "+";
	gint hours, minutes, seconds;

	if (utc_offset < 0) {
		utc_offset = -utc_offset;
		sign = "-";
	}

	hours = utc_offset / 3600;
	minutes = (utc_offset % 3600) / 60;
	seconds = utc_offset % 60;

	/* Sanity check. Standard timezone offsets shouldn't be much more
	 * than 12 hours, and daylight saving shouldn't change it by more
	 * than a few hours.  (The maximum offset is 15 hours 56 minutes
	 * at present.) */
	if (hours < 0 || hours >= 24 || minutes < 0 || minutes >= 60
	    || seconds < 0 || seconds >= 60) {
		fprintf (
			stderr, "Warning: Strange timezone offset: "
			"H:%i M:%i S:%i\n", hours, minutes, seconds);
	}

	if (hours == 0 && minutes == 0 && seconds == 0)
		strcpy (buffer, _("UTC"));
	else if (seconds == 0)
		sprintf (
			buffer, "%s %s%02i:%02i",
			_("UTC"), sign, hours, minutes);
	else
		sprintf (
			buffer, "%s %s%02i:%02i:%02i",
			_("UTC"), sign, hours, minutes, seconds);
}

static gchar *
zone_display_name_with_offset (icaltimezone *zone)
{
	const gchar *display_name;
	struct tm local;
	struct icaltimetype tt;
	gint offset;
	gchar buffer[100];
	time_t now = time (NULL);

	gmtime_r ((const time_t *) &now, &local);
	tt = tm_to_icaltimetype (&local, TRUE);
	offset = icaltimezone_get_utc_offset (zone, &tt, NULL);

	format_utc_offset (offset, buffer);

	display_name = icaltimezone_get_display_name (zone);
	if (icaltimezone_get_builtin_timezone (display_name))
		display_name = _(display_name);

	return g_strdup_printf ("%s (%s)", display_name, buffer);
}

static const gchar *
zone_display_name (icaltimezone *zone)
{
	const gchar *display_name;

	display_name = icaltimezone_get_display_name (zone);
	if (icaltimezone_get_builtin_timezone (display_name))
		display_name = _(display_name);

	return display_name;
}

/* This flashes the currently selected timezone in the map. */
static gboolean
on_map_timeout (gpointer data)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;

	etd = E_TIMEZONE_DIALOG (data);
	priv = etd->priv;

	if (!priv->point_selected)
		return TRUE;

	if (e_map_point_get_color_rgba (priv->point_selected)
	    == E_TIMEZONE_DIALOG_MAP_POINT_SELECTED_1_RGBA)
		e_map_point_set_color_rgba (
			priv->map, priv->point_selected,
			E_TIMEZONE_DIALOG_MAP_POINT_SELECTED_2_RGBA);
	else
		e_map_point_set_color_rgba (
			priv->map, priv->point_selected,
			E_TIMEZONE_DIALOG_MAP_POINT_SELECTED_1_RGBA);

	return TRUE;
}

static gboolean
on_map_motion (GtkWidget *widget,
               GdkEventMotion *event,
               gpointer data)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;
	gdouble longitude, latitude;
	icaltimezone *new_zone;
	gchar *display = NULL;

	etd = E_TIMEZONE_DIALOG (data);
	priv = etd->priv;

	e_map_window_to_world (
		priv->map, (gdouble) event->x, (gdouble) event->y,
		&longitude, &latitude);

	if (priv->point_hover && priv->point_hover != priv->point_selected)
		e_map_point_set_color_rgba (
			priv->map, priv->point_hover,
			E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);

	priv->point_hover = e_map_get_closest_point (
		priv->map, longitude,
		latitude, TRUE);

	if (priv->point_hover != priv->point_selected)
		e_map_point_set_color_rgba (
			priv->map, priv->point_hover,
			E_TIMEZONE_DIALOG_MAP_POINT_HOVER_RGBA);

	new_zone = get_zone_from_point (etd, priv->point_hover);

	display = zone_display_name_with_offset (new_zone);
	gtk_label_set_text (GTK_LABEL (priv->preview_label), display);

	g_free (display);

	return TRUE;
}

static gboolean
on_map_leave (GtkWidget *widget,
              GdkEventCrossing *event,
              gpointer data)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;

	etd = E_TIMEZONE_DIALOG (data);
	priv = etd->priv;

	/* We only want to reset the hover point if this is a normal leave
	 * event. For some reason we are getting leave events when the
	 * button is pressed in the map, which causes problems. */
	if (event->mode != GDK_CROSSING_NORMAL)
		return FALSE;

	if (priv->point_hover && priv->point_hover != priv->point_selected)
		e_map_point_set_color_rgba (
			priv->map, priv->point_hover,
			E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);

	timezone_combo_set_active_text (etd, zone_display_name (priv->zone));
	gtk_label_set_text (GTK_LABEL (priv->preview_label), "");

	priv->point_hover = NULL;

	return FALSE;
}

static gboolean
on_map_visibility_changed (GtkWidget *w,
                           GdkEventVisibility *event,
                           gpointer data)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;

	etd = E_TIMEZONE_DIALOG (data);
	priv = etd->priv;

	if (event->state != GDK_VISIBILITY_FULLY_OBSCURED) {
		/* Map is visible, at least partly, so make sure we flash the
		 * selected point. */
		if (priv->timeout_id == 0) {
			priv->timeout_id = e_named_timeout_add (
				100, on_map_timeout, etd);
		}
	} else {
		/* Map is invisible, so don't waste resources on the timeout.*/
		if (priv->timeout_id > 0) {
			g_source_remove (priv->timeout_id);
			priv->timeout_id = 0;
		}
	}

	return FALSE;
}

static gboolean
on_map_button_pressed (GtkWidget *w,
                       GdkEvent *button_event,
                       gpointer data)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;
	guint event_button = 0;
	gdouble event_x_win = 0;
	gdouble event_y_win = 0;
	gdouble longitude, latitude;

	etd = E_TIMEZONE_DIALOG (data);
	priv = etd->priv;

	gdk_event_get_button (button_event, &event_button);
	gdk_event_get_coords (button_event, &event_x_win, &event_y_win);

	e_map_window_to_world (
		priv->map, event_x_win, event_y_win, &longitude, &latitude);

	if (event_button != 1) {
		e_map_zoom_out (priv->map);
	} else {
		if (e_map_get_magnification (priv->map) <= 1.0)
			e_map_zoom_to_location (
				priv->map, longitude, latitude);

		if (priv->point_selected)
			e_map_point_set_color_rgba (
				priv->map,
				priv->point_selected,
				E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);
		priv->point_selected = priv->point_hover;

		priv->zone = get_zone_from_point (etd, priv->point_selected);
		timezone_combo_set_active_text (etd, zone_display_name (priv->zone));
	}

	return TRUE;
}

/* Returns the translated timezone location of the given EMapPoint,
 * e.g. "Europe/London". */
static icaltimezone *
get_zone_from_point (ETimezoneDialog *etd,
                     EMapPoint *point)
{
	icalarray *zones;
	gdouble longitude, latitude;
	gint i;

	if (point == NULL)
		return NULL;

	e_map_point_get_location (point, &longitude, &latitude);

	/* Get the array of builtin timezones. */
	zones = icaltimezone_get_builtin_timezones ();

	for (i = 0; i < zones->num_elements; i++) {
		icaltimezone *zone;
		gdouble zone_longitude, zone_latitude;

		zone = icalarray_element_at (zones, i);
		zone_longitude = icaltimezone_get_longitude (zone);
		zone_latitude = icaltimezone_get_latitude (zone);

		if (zone_longitude - 0.005 <= longitude &&
		    zone_longitude + 0.005 >= longitude &&
		    zone_latitude - 0.005 <= latitude &&
		    zone_latitude + 0.005 >= latitude)
		{
			return zone;
		}
	}

	g_return_val_if_reached (NULL);
}

/**
 * e_timezone_dialog_get_timezone:
 * @etd: the timezone dialog
 *
 * Return value: the currently-selected timezone, or %NULL if no timezone
 * is selected.
 **/
icaltimezone *
e_timezone_dialog_get_timezone (ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;

	g_return_val_if_fail (E_IS_TIMEZONE_DIALOG (etd), NULL);

	priv = etd->priv;

	return priv->zone;
}

/**
 * e_timezone_dialog_set_timezone:
 * @etd: the timezone dialog
 * @zone: the timezone
 *
 * Sets the timezone of @etd to @zone. Updates the display name and
 * selected location. The caller must ensure that @zone is not freed
 * before @etd is destroyed.
 **/

void
e_timezone_dialog_set_timezone (ETimezoneDialog *etd,
                                icaltimezone *zone)
{
	ETimezoneDialogPrivate *priv;
	gchar *display = NULL;

	g_return_if_fail (E_IS_TIMEZONE_DIALOG (etd));

	if (!zone)
		zone = get_local_timezone ();

	if (zone)
		display = zone_display_name_with_offset (zone);

	/* Add any unknown/custom timezone with defined location */
	if (zone && icaltimezone_get_location (zone) &&
	    !g_hash_table_lookup (etd->priv->index, icaltimezone_get_location (zone))) {
		GtkTreeStore *tree_store;
		GtkTreeIter *piter, iter;
		const gchar *location;

		location = icaltimezone_get_location (zone);
		tree_store = GTK_TREE_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (etd->priv->timezone_combo)));

		gtk_tree_store_prepend (tree_store, &iter, NULL);
		gtk_tree_store_set (tree_store, &iter, 0, (gchar *) location, 1, (gchar *) location, -1);

		piter = g_new (GtkTreeIter, 1);
		*piter = iter;

		g_hash_table_insert (etd->priv->index, (gchar *) location, piter);

		etd->priv->custom_zones = g_slist_prepend (etd->priv->custom_zones, zone);
	}

	priv = etd->priv;

	priv->zone = zone;

	gtk_label_set_text (
		GTK_LABEL (priv->preview_label),
		zone ? display : "");
	timezone_combo_set_active_text (etd, zone ? zone_display_name (zone) : "");

	set_map_timezone (etd, zone);
	g_free (display);
}

GtkWidget *
e_timezone_dialog_get_toplevel (ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;

	g_return_val_if_fail (etd != NULL, NULL);
	g_return_val_if_fail (E_IS_TIMEZONE_DIALOG (etd), NULL);

	priv = etd->priv;

	return priv->app;
}

static void
set_map_timezone (ETimezoneDialog *etd,
                  icaltimezone *zone)
{
	ETimezoneDialogPrivate *priv;
	EMapPoint *point;
	gdouble zone_longitude, zone_latitude;

	priv = etd->priv;

	if (zone) {
		zone_longitude = icaltimezone_get_longitude (zone);
		zone_latitude = icaltimezone_get_latitude (zone);
		point = e_map_get_closest_point (
			priv->map,
			zone_longitude,
			zone_latitude,
			FALSE);
	} else
		point = NULL;

	if (priv->point_selected)
		e_map_point_set_color_rgba (
			priv->map, priv->point_selected,
			E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);

	priv->point_selected = point;
}

static void
on_combo_changed (GtkComboBox *combo_box,
                  ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;
	gchar *new_zone_name;
	icalarray *zones;
	icaltimezone *map_zone = NULL;
	gchar *location;
	gint i;

	priv = etd->priv;

	timezone_combo_get_active_text (
		GTK_COMBO_BOX (priv->timezone_combo), &new_zone_name);

	if (!new_zone_name || !*new_zone_name)
		priv->zone = NULL;
	else if (!g_utf8_collate (new_zone_name, _("UTC")))
		priv->zone = icaltimezone_get_utc_timezone ();
	else {
		priv->zone = NULL;

		zones = icaltimezone_get_builtin_timezones ();
		for (i = 0; i < zones->num_elements; i++) {
			map_zone = icalarray_element_at (zones, i);
			location = _(icaltimezone_get_location (map_zone));
			if (!g_utf8_collate (new_zone_name, location)) {
				priv->zone = map_zone;
				break;
			}
		}

		if (!priv->zone) {
			GSList *link;

			for (link = priv->custom_zones; link; link = g_slist_next (link)) {
				icaltimezone *zone = link->data;

				if (zone && g_utf8_collate (new_zone_name, _(icaltimezone_get_location (zone))) == 0) {
					map_zone = zone;
					priv->zone = zone;
					break;
				}
			}
		}
	}

	set_map_timezone (etd, map_zone);

	g_free (new_zone_name);
}

static void
timezone_combo_get_active_text (GtkComboBox *combo,
                                gchar **zone_name)
{
	GtkTreeModel *tree_store;
	GtkTreeIter iter;

	tree_store = gtk_combo_box_get_model (combo);

	/* Get the active iter in the list */
	if (gtk_combo_box_get_active_iter (combo, &iter))
		gtk_tree_model_get (tree_store, &iter, 1, zone_name, -1);
	else
		*zone_name = NULL;
}

static gboolean
timezone_combo_set_active_text (ETimezoneDialog *etd,
                                const gchar *zone_name)
{
	GtkTreeIter *piter = NULL;
	GtkComboBox *combo;

	combo = GTK_COMBO_BOX (etd->priv->timezone_combo);

	if (zone_name && *zone_name)
		piter = g_hash_table_lookup (etd->priv->index, zone_name);

	if (piter)
		gtk_combo_box_set_active_iter (combo, piter);
	else
		gtk_combo_box_set_active (combo, 0);

	return piter != NULL;
}

static void
map_destroy_cb (gpointer data,
                GObject *where_object_was)
{

	ETimezoneDialogPrivate *priv = data;
	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}
	return;
}
