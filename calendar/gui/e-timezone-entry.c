/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * ETimezoneEntry - a field for setting a timezone. It shows the timezone in
 * a GtkEntry with a '...' button beside it which shows a dialog for changing
 * the timezone. The dialog contains a map of the world with a point for each
 * timezone, and an option menu as an alternative way of selecting the
 * timezone.
 */

#include <config.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtksignal.h>
#include <gnome.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>
#include <widgets/e-timezone-dialog/e-timezone-dialog.h>
#include "e-timezone-entry.h"

/* The timezone icon for the button. */
#include "art/timezone-16.xpm"

struct _ETimezoneEntryPrivate {
	/* This is the timezone set in e_timezone_entry_set_timezone().
	   Note that we don't copy it or use a ref count - we assume it is
	   never destroyed for the lifetime of this widget. */
	icaltimezone *zone;
	
	/* This is TRUE if the timezone has been changed since being set.
	   If it hasn't, we can just return zone, If it has, we return the
	   builtin timezone with tzid. (It can only be changed to a builtin
	   timezone, or to 'local time', i.e. no timezone.) */
	gboolean changed;

	GtkWidget *entry;
	GtkWidget *button;

	/* This can be set to the default timezone. If the current timezone
	   setting in the ETimezoneEntry matches this, then the entry field
	   is hidden. This makes the user interface simpler. */
	icaltimezone *default_zone;
};


enum {
  CHANGED,
  LAST_SIGNAL
};


static void e_timezone_entry_class_init	(ETimezoneEntryClass	*class);
static void e_timezone_entry_init	(ETimezoneEntry	*tentry);
static void e_timezone_entry_destroy	(GtkObject	*object);

static void on_entry_changed		(GtkEntry	*entry,
					 ETimezoneEntry *tentry);
static void on_button_clicked		(GtkWidget	*widget,
					 ETimezoneEntry	*tentry);

static char* e_timezone_entry_get_display_name	(ETimezoneEntry *tentry,
						 icaltimezone	*zone);

static void e_timezone_entry_set_entry_visibility (ETimezoneEntry *tentry);


static GtkHBoxClass *parent_class;
static guint timezone_entry_signals[LAST_SIGNAL] = { 0 };

E_MAKE_TYPE (e_timezone_entry, "ETimezoneEntry", ETimezoneEntry,
	     e_timezone_entry_class_init, e_timezone_entry_init, GTK_TYPE_HBOX);

static void
e_timezone_entry_class_init		(ETimezoneEntryClass	*class)
{
	GtkObjectClass *object_class = (GtkObjectClass *) class;

	object_class = (GtkObjectClass*) class;

	parent_class = g_type_class_peek_parent (class);

	timezone_entry_signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				G_TYPE_FROM_CLASS (object_class),
				GTK_SIGNAL_OFFSET (ETimezoneEntryClass,
						   changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);


	object_class->destroy		= e_timezone_entry_destroy;

	class->changed = NULL;
}


static void
e_timezone_entry_init		(ETimezoneEntry	*tentry)
{
	ETimezoneEntryPrivate *priv;
	GdkColormap *colormap;
	GdkPixmap *timezone_icon;
	GdkBitmap *timezone_mask;
	GtkWidget *pixmap;

	tentry->priv = priv = g_new0 (ETimezoneEntryPrivate, 1);

	priv->zone = NULL;
	priv->changed = FALSE;
	priv->default_zone = NULL;

	priv->entry  = gtk_entry_new ();
	gtk_entry_set_editable (GTK_ENTRY (priv->entry), FALSE);
	/*gtk_widget_set_usize (priv->date_entry, 90, 0);*/
	gtk_box_pack_start (GTK_BOX (tentry), priv->entry, TRUE, TRUE, 0);
	gtk_widget_show (priv->entry);
	g_signal_connect (priv->entry, "changed", G_CALLBACK (on_entry_changed), tentry);
	
	priv->button = gtk_button_new ();
	g_signal_connect (priv->button, "clicked", G_CALLBACK (on_button_clicked), tentry);
	gtk_box_pack_start (GTK_BOX (tentry), priv->button, FALSE, FALSE, 0);
	gtk_widget_show (priv->button);

	colormap = gtk_widget_get_colormap (priv->button);
	timezone_icon = gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &timezone_mask, NULL, timezone_16_xpm);

	pixmap = gtk_pixmap_new (timezone_icon, timezone_mask);
	gtk_container_add (GTK_CONTAINER (priv->button), pixmap);
	gtk_widget_show (pixmap);
}


/**
 * e_timezone_entry_new:
 *
 * Description: Creates a new #ETimezoneEntry widget which can be used
 * to provide an easy to use way for entering dates and times.
 * 
 * Returns: a new #ETimezoneEntry widget.
 */
GtkWidget *
e_timezone_entry_new			(void)
{
	ETimezoneEntry *tentry;

	tentry = g_object_new (e_timezone_entry_get_type (), NULL);

	return GTK_WIDGET (tentry);
}


static void
e_timezone_entry_destroy		(GtkObject	*object)
{
	ETimezoneEntry *tentry;
	ETimezoneEntryPrivate *priv;

	g_return_if_fail (E_IS_TIMEZONE_ENTRY (object));

	tentry = E_TIMEZONE_ENTRY (object);
	priv = tentry->priv;

	g_free (tentry->priv);
	tentry->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* The arrow button beside the date field has been clicked, so we show the
   popup with the ECalendar in. */
static void
on_button_clicked		(GtkWidget	*widget,
				 ETimezoneEntry	*tentry)
{
	ETimezoneEntryPrivate *priv;
	ETimezoneDialog *timezone_dialog;
	GtkWidget *dialog;
	char *tzid = NULL;
	const gchar *old_display_name;
	const gchar *display_name;

	priv = tentry->priv;
	display_name = gtk_entry_get_text (GTK_ENTRY (priv->entry));

	if (priv->zone)
		tzid = icaltimezone_get_tzid (priv->zone);

	timezone_dialog = e_timezone_dialog_new ();

	/* e_timezone_dialog_set_timezone() should really take (const gchar *) */
	e_timezone_dialog_set_timezone (timezone_dialog, tzid, (gchar *) display_name);

	dialog = e_timezone_dialog_get_toplevel (timezone_dialog);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		tzid = e_timezone_dialog_get_timezone (E_TIMEZONE_DIALOG (timezone_dialog), &display_name);
		old_display_name = gtk_entry_get_text (GTK_ENTRY (priv->entry));
		/* See if the timezone has been changed. It can only have been
		   changed to a builtin timezone, in which case the returned
		   TZID will be NULL. */
		if (strcmp (old_display_name, display_name)
		    || (!tzid && priv->zone)) {
			priv->changed = TRUE;
			priv->zone = NULL;
		}

		gtk_entry_set_text (GTK_ENTRY (priv->entry), display_name);
		e_timezone_entry_set_entry_visibility (tentry);
	}

	g_object_unref (timezone_dialog);
}


static void
on_entry_changed			(GtkEntry	*entry,
					 ETimezoneEntry *tentry)
{
	gtk_signal_emit (GTK_OBJECT (tentry), timezone_entry_signals[CHANGED]);
}


icaltimezone*
e_timezone_entry_get_timezone		(ETimezoneEntry	*tentry)
{
	ETimezoneEntryPrivate *priv;
	const char *display_name;

	g_return_val_if_fail (E_IS_TIMEZONE_ENTRY (tentry), NULL);

	priv = tentry->priv;

	/* If the timezone hasn't been change, we can just return the same
	   zone we were passed in. */
	if (!priv->changed)
		return priv->zone;

	/* If the timezone has changed, it can only have been changed to a
	   builtin timezone or 'local time' (i.e. no timezone). */
	display_name = gtk_entry_get_text (GTK_ENTRY (priv->entry));

	return e_timezone_dialog_get_builtin_timezone (display_name);
}


void
e_timezone_entry_set_timezone		(ETimezoneEntry	*tentry,
					 icaltimezone	*zone)
{
	ETimezoneEntryPrivate *priv;
	gchar *display_name;

	g_return_if_fail (E_IS_TIMEZONE_ENTRY (tentry));

	priv = tentry->priv;

	priv->zone = zone;
	priv->changed = FALSE;

	display_name = e_timezone_entry_get_display_name (tentry, zone);
	gtk_entry_set_text (GTK_ENTRY (priv->entry),
			    display_name ? display_name : "");
	g_free (display_name);

	e_timezone_entry_set_entry_visibility (tentry);
}


/* Returns the timezone name to display to the user, in the locale's encoding.
   We prefer to use the Olson city name, but fall back on the TZNAME, or
   finally the TZID. We don't want to use "" as it may be wrongly interpreted
   as a 'local time'. If zone is NULL, NULL is returned. The returned string
   should be freed. */
static char*
e_timezone_entry_get_display_name	(ETimezoneEntry *tentry,
					 icaltimezone	*zone)
{
	char *display_name;

	if (!zone)
		return NULL;

	/* Get the UTF-8 display name from the icaltimezone. */
	display_name = icaltimezone_get_display_name (zone);

	/* We check if it is one of our builtin timezone names, in which case
	   we call gettext to translate it. If it isn't a builtin timezone
	   name, we need to convert it to the GTK+ encoding. */
	if (icaltimezone_get_builtin_timezone (display_name)) {
		return g_strdup (_(display_name));
	} else {
		return e_utf8_to_gtk_string (GTK_WIDGET (tentry),
					     display_name);
	}
}


/* Sets the default timezone. If the current timezone matches this, then the
   entry field is hidden. This is useful since most people do not use timezones
   so it makes the user interface simpler. */
void
e_timezone_entry_set_default_timezone	(ETimezoneEntry	*tentry,
					 icaltimezone	*zone)
{
	ETimezoneEntryPrivate *priv;

	g_return_if_fail (E_IS_TIMEZONE_ENTRY (tentry));

	priv = tentry->priv;

	priv->default_zone = zone;

	e_timezone_entry_set_entry_visibility (tentry);
}


static void
e_timezone_entry_set_entry_visibility (ETimezoneEntry	*tentry)
{
	ETimezoneEntryPrivate *priv;
	icaltimezone *zone;
	gboolean show_entry = TRUE;

	priv = tentry->priv;

	if (priv->default_zone) {
		zone = e_timezone_entry_get_timezone (tentry);
		if (zone == priv->default_zone)
			show_entry = FALSE;
	}

	if (show_entry)
		gtk_widget_show (priv->entry);
	else
		gtk_widget_hide (priv->entry);
}

