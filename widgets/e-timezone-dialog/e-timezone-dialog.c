/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Timezone selector dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Damon Chaplin <damon@ximian.com>
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
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkcombo.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gnome.h>
#include <glade/glade.h>
#include <widgets/misc/e-map.h>

#include "e-timezone-dialog.h"

#include <gal/util/e-util.h>

#define E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA 0xc070a0ff
#define E_TIMEZONE_DIALOG_MAP_POINT_HOVER_RGBA 0xffff60ff
#define E_TIMEZONE_DIALOG_MAP_POINT_SELECTED_1_RGBA 0xff60e0ff
#define E_TIMEZONE_DIALOG_MAP_POINT_SELECTED_2_RGBA 0x000000ff

struct _ETimezoneDialogPrivate {
	/* The selected timezone. May be NULL for a 'local time' (i.e. when
	   the displayed name is ""). */
	icaltimezone *zone;

	/* Glade XML data */
	GladeXML *xml;

	EMapPoint *point_selected;
	EMapPoint *point_hover;

	EMap *map;

	/* The timeout used to flash the nearest point. */
	guint timeout_id;

	/* Widgets from the Glade file */
	GtkWidget *app;
	GtkWidget *table;
	GtkWidget *map_window;
	GtkWidget *timezone_combo;
};


static void e_timezone_dialog_class_init	(ETimezoneDialogClass *class);
static void e_timezone_dialog_init		(ETimezoneDialog      *etd);
static void e_timezone_dialog_dispose		(GObject	*object);
static void e_timezone_dialog_finalize		(GObject	*object);

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
						 GdkEventButton *event,
						 gpointer	 data);

static icaltimezone* get_zone_from_point	(ETimezoneDialog *etd,
						 EMapPoint	*point);
static void	set_map_timezone		(ETimezoneDialog *etd,
						 icaltimezone    *zone);
static void	on_combo_changed		(GtkEditable	*entry,
						 ETimezoneDialog *etd);


static GObjectClass *parent_class;


/* Class initialization function for the event editor */
static void
e_timezone_dialog_class_init (ETimezoneDialogClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose  = e_timezone_dialog_dispose;
	object_class->finalize = e_timezone_dialog_finalize;

	parent_class = gtk_type_class (G_TYPE_OBJECT);
}

/* Object initialization function for the event editor */
static void
e_timezone_dialog_init (ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;

	priv = g_new0 (ETimezoneDialogPrivate, 1);
	etd->priv = priv;

	priv->point_selected = NULL;
	priv->point_hover = NULL;
	priv->timeout_id = 0;
}

/* Dispose handler for the event editor */
static void
e_timezone_dialog_dispose (GObject *object)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_TIMEZONE_DIALOG (object));

	etd = E_TIMEZONE_DIALOG (object);
	priv = etd->priv;

	/* Destroy the actual dialog. */
	if (priv->app != NULL) {
		gtk_widget_destroy (priv->app);
		priv->app = NULL;
	}

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	if (priv->xml) {
		g_object_unref (priv->xml);
		priv->xml = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

/* Finalize handler for the event editor */
static void
e_timezone_dialog_finalize (GObject *object)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_TIMEZONE_DIALOG (object));

	etd = E_TIMEZONE_DIALOG (object);
	priv = etd->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
e_timezone_dialog_add_timezones (ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;
	icalarray *zones;
	GtkWidget *listitem;
	GtkCombo *combo;
	int i;

	priv = etd->priv;

	combo = GTK_COMBO (priv->timezone_combo);

	/* Clear any existing items in the combo. */
	gtk_list_clear_items (GTK_LIST (combo->list), 0, -1);

	/* Put the "None" and "UTC" entries at the top of the combo's list.
	   When "None" is selected we want the field to be cleared. */
	listitem = gtk_list_item_new_with_label (_("None"));
	gtk_combo_set_item_string (combo, GTK_ITEM (listitem), "");
	gtk_widget_show (listitem);
	gtk_container_add (GTK_CONTAINER (combo->list), listitem);

	listitem = gtk_list_item_new_with_label (_("UTC"));
	gtk_widget_show (listitem);
	gtk_container_add (GTK_CONTAINER (combo->list), listitem);

	/* Get the array of builtin timezones. */
	zones = icaltimezone_get_builtin_timezones ();

	for (i = 0; i < zones->num_elements; i++) {
		icaltimezone *zone;
		char *location;

		zone = icalarray_element_at (zones, i);

		location = _(icaltimezone_get_location (zone));

		e_map_add_point (priv->map, location,
				 icaltimezone_get_longitude (zone),
				 icaltimezone_get_latitude (zone),
				 E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);

		listitem = gtk_list_item_new_with_label (location);
		gtk_widget_show (listitem);
		gtk_container_add (GTK_CONTAINER (combo->list), listitem);
	}
}


ETimezoneDialog *
e_timezone_dialog_construct (ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;
	GtkWidget *map;

	g_return_val_if_fail (etd != NULL, NULL);
	g_return_val_if_fail (E_IS_TIMEZONE_DIALOG (etd), NULL);

	priv = etd->priv;

	/* Load the content widgets */

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/e-timezone-dialog.glade", NULL, NULL);
	if (!priv->xml) {
		g_message ("e_timezone_dialog_construct(): Could not load the Glade XML file!");
		goto error;
	}

	if (!get_widgets (etd)) {
		g_message ("e_timezone_dialog_construct(): Could not find all widgets in the XML file!");
		goto error;
	}

	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (priv->app)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (priv->app)->action_area), 12);

	priv->map = e_map_new ();
	map = GTK_WIDGET (priv->map);
	gtk_widget_set_events (map, gtk_widget_get_events (map)
			       | GDK_LEAVE_NOTIFY_MASK
			       | GDK_VISIBILITY_NOTIFY_MASK);

	gtk_entry_set_editable (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry), FALSE);

	e_timezone_dialog_add_timezones (etd);

	gtk_container_add (GTK_CONTAINER (priv->map_window), map);
	gtk_widget_show (map);

	/* Ensure a reasonable minimum amount of map is visible */
	gtk_widget_set_size_request (priv->map_window, 200, 200);

        g_signal_connect (map, "motion-notify-event", G_CALLBACK (on_map_motion), etd);
        g_signal_connect (map, "leave-notify-event", G_CALLBACK (on_map_leave), etd);
        g_signal_connect (map, "visibility-notify-event", G_CALLBACK (on_map_visibility_changed), etd);
	g_signal_connect (map, "button-press-event", G_CALLBACK (on_map_button_pressed), etd);

	g_signal_connect (GTK_COMBO (priv->timezone_combo)->entry, "activate", G_CALLBACK (on_combo_changed), etd);

	return etd;

 error:

	g_object_unref (etd);
	return NULL;
}

/* Gets the widgets from the XML file and returns if they are all available.
 * For the widgets whose values can be simply set with e-dialog-utils, it does
 * that as well.
 */
static gboolean
get_widgets (ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;

	priv = etd->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->app		= GW ("timezone-dialog");
	priv->map_window	= GW ("map-window");
	priv->timezone_combo	= GW ("timezone-combo");
	priv->table             = GW ("timezone-table");

	return (priv->app
		&& priv->map_window
		&& priv->timezone_combo
		&& priv->table);
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


static const char *
zone_display_name (icaltimezone *zone)
{
	const char *display_name;

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
	        e_map_point_set_color_rgba (priv->map, priv->point_selected,
					    E_TIMEZONE_DIALOG_MAP_POINT_SELECTED_2_RGBA);
	else
	        e_map_point_set_color_rgba (priv->map, priv->point_selected,
					    E_TIMEZONE_DIALOG_MAP_POINT_SELECTED_1_RGBA);

	return TRUE;
}


static gboolean
on_map_motion (GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;
	double longitude, latitude;
	icaltimezone *new_zone;

	etd = E_TIMEZONE_DIALOG (data);
	priv = etd->priv;

	e_map_window_to_world (priv->map, (double) event->x, (double) event->y,
			       &longitude, &latitude);

	if (priv->point_hover && priv->point_hover != priv->point_selected)
	        e_map_point_set_color_rgba (priv->map, priv->point_hover,
					    E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);

	priv->point_hover = e_map_get_closest_point (priv->map, longitude,
						     latitude, TRUE);

	if (priv->point_hover != priv->point_selected)
	        e_map_point_set_color_rgba (priv->map, priv->point_hover,
					    E_TIMEZONE_DIALOG_MAP_POINT_HOVER_RGBA);

	new_zone = get_zone_from_point (etd, priv->point_hover);

	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry),
			    zone_display_name (new_zone));

	return TRUE;
}


static gboolean
on_map_leave (GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;

	etd = E_TIMEZONE_DIALOG (data);
	priv = etd->priv;

	/* We only want to reset the hover point if this is a normal leave
	   event. For some reason we are getting leave events when the
	   button is pressed in the map, which causes problems. */
	if (event->mode != GDK_CROSSING_NORMAL)
		return FALSE;

	if (priv->point_hover && priv->point_hover != priv->point_selected)
	        e_map_point_set_color_rgba (priv->map, priv->point_hover,
					    E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);

	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry),
			    zone_display_name (priv->zone));

	priv->point_hover = NULL;

	return FALSE;
}


static gboolean
on_map_visibility_changed (GtkWidget *w, GdkEventVisibility *event,
			   gpointer data)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;
	
	etd = E_TIMEZONE_DIALOG (data);
	priv = etd->priv;

	if (event->state != GDK_VISIBILITY_FULLY_OBSCURED) {
		/* Map is visible, at least partly, so make sure we flash the
		   selected point. */
		if (!priv->timeout_id)
			priv->timeout_id = gtk_timeout_add (100, on_map_timeout, etd);
	} else {
		/* Map is invisible, so don't waste resources on the timeout.*/
		if (priv->timeout_id) {
			g_source_remove (priv->timeout_id);
			priv->timeout_id = 0;
		}
	}

	return FALSE;
}


static gboolean
on_map_button_pressed (GtkWidget *w, GdkEventButton *event, gpointer data)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;
	double longitude, latitude;
	
	etd = E_TIMEZONE_DIALOG (data);
	priv = etd->priv;

	e_map_window_to_world (priv->map, (double) event->x, (double) event->y,
			       &longitude, &latitude);
	
	if (event->button != 1) {
	        e_map_zoom_out (priv->map);
	} else {
		if (e_map_get_magnification (priv->map) <= 1.0)
		        e_map_zoom_to_location (priv->map, longitude,
						latitude);
	
		if (priv->point_selected)
		        e_map_point_set_color_rgba (priv->map,
						    priv->point_selected,
						    E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);
		priv->point_selected = priv->point_hover;
		
		priv->zone = get_zone_from_point (etd, priv->point_selected);
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry),
				    zone_display_name (priv->zone));
	}
	
	return TRUE;
}


/* Returns the translated timezone location of the given EMapPoint,
   e.g. "Europe/London". */
static icaltimezone *
get_zone_from_point (ETimezoneDialog *etd,
		     EMapPoint *point)
{
	ETimezoneDialogPrivate *priv;
	icalarray *zones;
	double longitude, latitude;
	int i;

	priv = etd->priv;

	if (point == NULL)
		return NULL;

	e_map_point_get_location (point, &longitude, &latitude);

	/* Get the array of builtin timezones. */
	zones = icaltimezone_get_builtin_timezones ();

	for (i = 0; i < zones->num_elements; i++) {
		icaltimezone *zone;
		double zone_longitude, zone_latitude;

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

	g_assert_not_reached ();

	return NULL;
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
				icaltimezone    *zone)
{
	ETimezoneDialogPrivate *priv;

	g_return_if_fail (E_IS_TIMEZONE_DIALOG (etd));

	priv = etd->priv;

	priv->zone = zone;

	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry),
			    zone ? zone_display_name (zone) : "");

	set_map_timezone (etd, zone);
}


GtkWidget *
e_timezone_dialog_get_toplevel	(ETimezoneDialog  *etd)
{
	ETimezoneDialogPrivate *priv;

	g_return_val_if_fail (etd != NULL, NULL);
	g_return_val_if_fail (E_IS_TIMEZONE_DIALOG (etd), NULL);

	priv = etd->priv;

	return priv->app;
}


static void
set_map_timezone (ETimezoneDialog *etd, icaltimezone *zone)
{
	ETimezoneDialogPrivate *priv;
	EMapPoint *point;
	double zone_longitude, zone_latitude;

	priv = etd->priv;

	if (zone) {
		zone_longitude = icaltimezone_get_longitude (zone);
		zone_latitude = icaltimezone_get_latitude (zone);
		point = e_map_get_closest_point (priv->map,
						 zone_longitude,
						 zone_latitude,
						 FALSE);
	} else
		point = NULL;

	if (priv->point_selected)
		e_map_point_set_color_rgba (priv->map, priv->point_selected,
					    E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);

	priv->point_selected = point;
}


static void
on_combo_changed (GtkEditable *entry, ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;
	const char *new_zone_name;
	icalarray *zones;
	icaltimezone *map_zone = NULL;
	char *location;
	int i;

	priv = etd->priv;

	new_zone_name = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry));

	if (!*new_zone_name)
		priv->zone = NULL;
	else if (!strcmp (new_zone_name, _("UTC")))
		priv->zone = icaltimezone_get_utc_timezone ();
	else {
		priv->zone = NULL;

		zones = icaltimezone_get_builtin_timezones ();
		for (i = 0; i < zones->num_elements; i++) {
			map_zone = icalarray_element_at (zones, i);
			location = _(icaltimezone_get_location (map_zone));
			if (!strcmp (new_zone_name, location)) {
				priv->zone = map_zone;
				break;
			}
		}
	}

	set_map_timezone (etd, map_zone);
}

/**
 * e_timezone_dialog_reparent:
 * @etd: #ETimezoneDialog.
 * @new_parent: The new parent widget.
 *
 * Takes the internal widgets out of the dialog and put them into @new_parent
 */
void
e_timezone_dialog_reparent (ETimezoneDialog *etd,
			    GtkWidget *new_parent)
{
	ETimezoneDialogPrivate *priv;

	priv = etd->priv;

	gtk_widget_reparent (priv->table, new_parent);
}

E_MAKE_TYPE (e_timezone_dialog, "ETimezoneDialog", ETimezoneDialog,
	     e_timezone_dialog_class_init, e_timezone_dialog_init, G_TYPE_OBJECT)
