/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Timezone selector dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Damon Chaplin <damon@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
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

#define E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA 0xc070a0ff
#define E_TIMEZONE_DIALOG_MAP_POINT_HOVER_RGBA 0xffff60ff
#define E_TIMEZONE_DIALOG_MAP_POINT_SELECTED_1_RGBA 0xff60e0ff
#define E_TIMEZONE_DIALOG_MAP_POINT_SELECTED_2_RGBA 0x000000ff

struct _ETimezoneDialogPrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Client to use */
	CalClient *client;

	GArray *zones;

	EMapPoint *point_selected;
	EMapPoint *point_hover;

	EMap *map;

	/* The timeout used to flash the nearest point. */
	guint timeout_id;

	/* Widgets from the Glade file */
	GtkWidget *app;
	GtkWidget *map_window;
	GtkWidget *timezone_preview;
	GtkWidget *timezone_combo;
};


static void e_timezone_dialog_class_init	(ETimezoneDialogClass *class);
static void e_timezone_dialog_init		(ETimezoneDialog      *etd);
static void e_timezone_dialog_destroy		(GtkObject	*object);

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


static GtkObjectClass *parent_class;


GtkType
e_timezone_dialog_get_type (void)
{
	static GtkType e_timezone_dialog_type = 0;

	if (!e_timezone_dialog_type) {
		static const GtkTypeInfo e_timezone_dialog_info = {
			"ETimezoneDialog",
			sizeof (ETimezoneDialog),
			sizeof (ETimezoneDialogClass),
			(GtkClassInitFunc) e_timezone_dialog_class_init,
			(GtkObjectInitFunc) e_timezone_dialog_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		e_timezone_dialog_type = gtk_type_unique (GTK_TYPE_OBJECT,
							  &e_timezone_dialog_info);
	}

	return e_timezone_dialog_type;
}

/* Class initialization function for the event editor */
static void
e_timezone_dialog_class_init (ETimezoneDialogClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	object_class->destroy = e_timezone_dialog_destroy;
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

/* Destroy handler for the event editor */
static void
e_timezone_dialog_destroy (GtkObject *object)
{
	ETimezoneDialog *etd;
	ETimezoneDialogPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_TIMEZONE_DIALOG (object));

	etd = E_TIMEZONE_DIALOG (object);
	priv = etd->priv;

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	if (priv->client) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->client), etd);
		gtk_object_unref (GTK_OBJECT (priv->client));
		priv->client = NULL;
	}

	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	g_free (priv);
	etd->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/e-timezone-dialog.glade",
				   NULL);
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

	gtk_container_add (GTK_CONTAINER (priv->map_window), map);
	gtk_widget_show (map);

        gtk_signal_connect (GTK_OBJECT (map), "motion-notify-event",
			    GTK_SIGNAL_FUNC (on_map_motion), etd);
        gtk_signal_connect (GTK_OBJECT (map), "leave-notify-event",
			    GTK_SIGNAL_FUNC (on_map_leave), etd);
        gtk_signal_connect (GTK_OBJECT (map), "visibility-notify-event",
			    GTK_SIGNAL_FUNC (on_map_visibility_changed), etd);
	gtk_signal_connect (GTK_OBJECT (map), "button-press-event",
			    GTK_SIGNAL_FUNC (on_map_button_pressed), etd);

	gtk_signal_connect (GTK_OBJECT (GTK_COMBO (priv->timezone_combo)->entry), "changed", 
			    GTK_SIGNAL_FUNC (on_combo_changed), etd);

	return etd;

 error:

	gtk_object_unref (GTK_OBJECT (etd));
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

	return (priv->app
		&& priv->map_window
		&& priv->timezone_combo
		&& priv->timezone_preview);
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

	etd = E_TIMEZONE_DIALOG (gtk_type_new (E_TYPE_TIMEZONE_DIALOG));
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
		
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry),
				    get_zone_from_point (etd, priv->point_selected));
	}
	
	return TRUE;
}


static char*
get_zone_from_point (ETimezoneDialog *etd,
		     EMapPoint *point)
{
	ETimezoneDialogPrivate *priv;
	CalTimezoneInfo *zone;
	double longitude, latitude;
	int i;

	priv = etd->priv;

	if (point == NULL || priv->zones == NULL)
		return "";

	e_map_point_get_location (point, &longitude, &latitude);

	for (i = 0; i < priv->zones->len; i++) {
		zone = &g_array_index (priv->zones, CalTimezoneInfo, i);

		if (zone->longitude - 0.005 <= longitude &&
		    zone->longitude + 0.005 >= longitude &&
		    zone->latitude - 0.005 <= latitude &&
		    zone->latitude + 0.005 >= latitude)
		{
			return zone->location;
		}
	}

	g_assert_not_reached ();

	return NULL;
}


CalClient*
e_timezone_dialog_get_cal_client	(ETimezoneDialog  *etd)
{

	return etd->priv->client;
}


void
e_timezone_dialog_set_cal_client	(ETimezoneDialog  *etd,
					 CalClient	  *client)
{
	ETimezoneDialogPrivate *priv;
	CalTimezoneInfo *zone;
	GtkWidget *listitem;
	GtkCombo *combo;
	char *current_zone;
	int i;

	g_return_if_fail (etd != NULL);
	g_return_if_fail (E_IS_TIMEZONE_DIALOG (etd));
	g_return_if_fail (IS_CAL_CLIENT (client));

	priv = etd->priv;

	combo = GTK_COMBO (priv->timezone_combo);

	/* Clear any existing items */
	gtk_list_clear_items (GTK_LIST (combo->list), 0, -1);

	priv->zones = cal_client_get_builtin_timezone_info (client);

	if (!priv->zones) {
		g_warning ("No timezone info found");
		return;
	}

	/* Put the "None" and "UTC" entries at the top of the combo's list.
	   When "None" is selected we want the field to be cleared. */
	listitem = gtk_list_item_new_with_label (_("None"));
	gtk_widget_show (listitem);
	gtk_container_add (GTK_CONTAINER (combo->list), listitem);
	gtk_combo_set_item_string (combo, GTK_ITEM (listitem), "");

	/* Note: We don't translate timezone names at the moment. */
	listitem = gtk_list_item_new_with_label ("UTC");
	gtk_widget_show (listitem);
	gtk_container_add (GTK_CONTAINER (combo->list), listitem);

	current_zone = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry));

	for (i = 0; i < priv->zones->len; i++) {
		zone = &g_array_index (priv->zones, CalTimezoneInfo, i);
		if (!strcmp (current_zone, zone->location)) {
			priv->point_selected = e_map_add_point (priv->map,
								zone->location,
								zone->longitude,
								zone->latitude,
								E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);
		} else {
			e_map_add_point (priv->map, zone->location,
					 zone->longitude, zone->latitude,
					 E_TIMEZONE_DIALOG_MAP_POINT_NORMAL_RGBA);
		}

		listitem = gtk_list_item_new_with_label (zone->location);
		gtk_widget_show (listitem);
		gtk_container_add (GTK_CONTAINER (combo->list), listitem);
	}
}


char*
e_timezone_dialog_get_timezone		(ETimezoneDialog  *etd)
{
	ETimezoneDialogPrivate *priv;

	g_return_val_if_fail (etd != NULL, NULL);
	g_return_val_if_fail (E_IS_TIMEZONE_DIALOG (etd), NULL);

	priv = etd->priv;

	return gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry));
}


void
e_timezone_dialog_set_timezone		(ETimezoneDialog  *etd,
					 char		  *timezone)
{
	ETimezoneDialogPrivate *priv;

	g_return_if_fail (etd != NULL);
	g_return_if_fail (E_IS_TIMEZONE_DIALOG (etd));

	priv = etd->priv;

	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry),
			    timezone);

	find_selected_point (etd);
}


GtkWidget*
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
	CalTimezoneInfo *zone;
	char *current_zone;
	EMapPoint *point = NULL;
	int i;

	priv = etd->priv;

	if (priv->zones == NULL)
		return;

	current_zone = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (priv->timezone_combo)->entry));

	for (i = 0; i < priv->zones->len; i++) {
		zone = &g_array_index (priv->zones, CalTimezoneInfo, i);
		if (!strcmp (current_zone, zone->location)) {
			point = e_map_get_closest_point (priv->map,
							 zone->longitude,
							 zone->latitude,
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
	find_selected_point (etd);
}
