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
	/* The TZID of the timezone. May be NULL for a 'local time' (i.e. when
	   the displayed name is "") or for builtin timezones which we haven't
	   loaded yet. */
	char *tzid;

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
	GtkWidget *timezone_preview;
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

static char*	get_zone_from_point		(ETimezoneDialog *etd,
						 EMapPoint	*point);
static void	find_selected_point		(ETimezoneDialog *etd);
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

	priv->tzid = NULL;
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
	GtkWidget *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_TIMEZONE_DIALOG (object));

	etd = E_TIMEZONE_DIALOG (object);
	priv = etd->priv;

	/* Destroy the actual dialog. */
	if (dialog != NULL) {
		dialog = e_timezone_dialog_get_toplevel (etd);
		gtk_widget_destroy (dialog);
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

	g_free (priv->tzid);
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

	map = GTK_WIDGET (e_map_new ());
	priv->map = E_MAP (map);
	gtk_widget_set_events (map, gtk_widget_get_events (map)
			       | GDK_LEAVE_NOTIFY_MASK
			       | GDK_VISIBILITY_NOTIFY_MASK);

	gtk_entry_set_editable (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry), FALSE);

	e_timezone_dialog_add_timezones (etd);

	gtk_container_add (GTK_CONTAINER (priv->map_window), map);
	gtk_widget_show (map);

        g_signal_connect (map, "motion-notify-event", G_CALLBACK (on_map_motion), etd);
        g_signal_connect (map, "leave-notify-event", G_CALLBACK (on_map_leave), etd);
        g_signal_connect (map, "visibility-notify-event", G_CALLBACK (on_map_visibility_changed), etd);
	g_signal_connect (map, "button-press-event", G_CALLBACK (on_map_button_pressed), etd);

	g_signal_connect (GTK_COMBO (priv->timezone_combo)->entry, "changed", G_CALLBACK (on_combo_changed), etd);

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
	priv->timezone_preview	= GW ("timezone-preview");
	priv->table             = GW ("table1");

	return (priv->app
		&& priv->map_window
		&& priv->timezone_combo
		&& priv->timezone_preview
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
	char *old_zone, *new_zone;

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

	gtk_label_get (GTK_LABEL (priv->timezone_preview), &old_zone);
	new_zone = get_zone_from_point (etd, priv->point_hover);
	if (strcmp (old_zone, new_zone))
		gtk_label_set_text (GTK_LABEL (priv->timezone_preview),
				    new_zone);

	return TRUE;
}


static gboolean
on_map_leave (GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;
	char *old_zone;

	etd = E_TIMEZONE_DIALOG (data);
	priv = etd->priv;

	/* We only want to reset the hover point and the preview text if this
	   is a normal leave event. For some reason we are getting leave events
	   when the button is pressed in the map, which causes problems. */
	if (event->mode != GDK_CROSSING_NORMAL)
		return FALSE;

	if (priv->point_hover && priv->point_hover != priv->point_selected)
	        e_map_point_set_color_rgba (priv->map, priv->point_hover,
					    E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);

	priv->point_hover = NULL;

	/* Clear the timezone preview label, if it isn't already empty. */
	gtk_label_get (GTK_LABEL (priv->timezone_preview), &old_zone);
	if (strcmp (old_zone, ""))
		gtk_label_set_text (GTK_LABEL (priv->timezone_preview), "");

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
	char *location;
	
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
		
		location = get_zone_from_point (etd, priv->point_selected);
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry),
				    location);

		g_free (priv->tzid);
		priv->tzid = NULL;
	}
	
	return TRUE;
}


/* Returns the translated timezone location of the fiven EMapPoint,
   e.g. "Europe/London", in the current locale's encoding (not UTF-8). */
static char*
get_zone_from_point (ETimezoneDialog *etd,
		     EMapPoint *point)
{
	ETimezoneDialogPrivate *priv;
	icalarray *zones;
	double longitude, latitude;
	int i;

	priv = etd->priv;

	if (point == NULL)
		return "";

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
			return _(icaltimezone_get_location (zone));
		}
	}

	g_assert_not_reached ();

	return NULL;
}


/* Returns the TZID of the timezone set, and optionally its displayed name.
   The TZID may be NULL, in which case the builtin timezone with the city name
   of display_name should be used. If display_name is also NULL or "", then it
   is assumed to be a 'local time'. Note that display_name may be translated,
   so you need to convert it back to English before trying to load it. 
   It will be in the GTK+ encoding, i.e. not UTF-8. */
char*
e_timezone_dialog_get_timezone		(ETimezoneDialog  *etd,
					 const char **display_name)
{
	ETimezoneDialogPrivate *priv;

	g_return_val_if_fail (etd != NULL, NULL);
	g_return_val_if_fail (E_IS_TIMEZONE_DIALOG (etd), NULL);

	priv = etd->priv;

	if (display_name)
		*display_name = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry));

	return priv->tzid;
}


/* Sets the TZID and displayed name of the timezone. The TZID may be NULL for
   a 'local time' (i.e. display_name is NULL or "") or if it is a builtin
   timezone which hasn't been loaded yet. (This is done so we don't load
   timezones until we really need them.) The display_name should be the
   translated name in the GTK+ - it will be displayed exactly as it is. */
void
e_timezone_dialog_set_timezone		(ETimezoneDialog  *etd,
					 char		  *tzid,
					 char		  *display_name)
{
	ETimezoneDialogPrivate *priv;

	g_return_if_fail (etd != NULL);
	g_return_if_fail (E_IS_TIMEZONE_DIALOG (etd));

	priv = etd->priv;

	if (priv->tzid)
		g_free (priv->tzid);

	priv->tzid = g_strdup (tzid);

	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry),
			    display_name ? display_name : "");

	find_selected_point (etd);
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


/* This tries to find the timezone corresponding to the text in the combo,
   and selects the point so that it flashes. */
static void
find_selected_point (ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;
	icalarray *zones;
	const char *current_zone;
	EMapPoint *point = NULL;
	int i;

	priv = etd->priv;

	current_zone = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry));

	/* Get the array of builtin timezones. */
	zones = icaltimezone_get_builtin_timezones ();

	for (i = 0; i < zones->num_elements; i++) {
		icaltimezone *zone;
		char *location;

		zone = icalarray_element_at (zones, i);

		location = _(icaltimezone_get_location (zone));

		if (!strcmp (current_zone, location)) {
			double zone_longitude, zone_latitude;

			zone_longitude = icaltimezone_get_longitude (zone);
			zone_latitude = icaltimezone_get_latitude (zone);

			point = e_map_get_closest_point (priv->map,
							 zone_longitude,
							 zone_latitude,
							 FALSE);
			break;
		}
	}

	if (priv->point_selected)
		e_map_point_set_color_rgba (priv->map, priv->point_selected,
					    E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);

	priv->point_selected = point;
}


static void
on_combo_changed (GtkEditable *entry, ETimezoneDialog *etd)
{
	ETimezoneDialogPrivate *priv;

	priv = etd->priv;

	find_selected_point (etd);

	g_free (priv->tzid);
	priv->tzid = NULL;
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


/* Returns the builtin timezone corresponding to display_name, which is
   the translated location, e.g. 'Europe/London', in the locale's encoding.
   If display_name is NULL or "" it returns NULL. */
icaltimezone*
e_timezone_dialog_get_builtin_timezone	(const char *display_name)
{
	icalarray *zones;
	int i;

	/* If the field is empty, return NULL (i.e. a floating time). */
	if (!display_name || !display_name[0])
		return NULL;

	/* Check for UTC. */
	if (!strcmp (display_name, _("UTC")))
		return icaltimezone_get_utc_timezone ();

	/* Get the array of builtin timezones. */
	zones = icaltimezone_get_builtin_timezones ();

	for (i = 0; i < zones->num_elements; i++) {
		icaltimezone *zone;
		char *location;

		zone = icalarray_element_at (zones, i);
		location = icaltimezone_get_location (zone);
		if (!strcmp (display_name, _(location)))
			return zone;
	}

	return NULL;
}


E_MAKE_TYPE (e_timezone_dialog, "ETimezoneDialog", ETimezoneDialog,
	     e_timezone_dialog_class_init, e_timezone_dialog_init, G_TYPE_OBJECT)
