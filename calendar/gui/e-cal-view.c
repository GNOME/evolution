/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2003, Ximian, Inc.
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

#include <config.h>
#include <gal/util/e-util.h>
#include "evolution-activity-client.h"
#include "e-cal-view.h"

/* Used for the status bar messages */
#define EVOLUTION_CALENDAR_PROGRESS_IMAGE "evolution-calendar-mini.png"
static GdkPixbuf *progress_icon[2] = { NULL, NULL };

struct _ECalViewPrivate {
	/* The GnomeCalendar we are associated to */
	GnomeCalendar *calendar;

	/* The activity client used to show messages on the status bar. */
	EvolutionActivityClient *activity;
};

static void e_cal_view_class_init (ECalViewClass *klass);
static void e_cal_view_init (ECalView *cal_view, ECalViewClass *klass);
static void e_cal_view_destroy (GtkObject *object);

static GObjectClass *parent_class = NULL;

/* Signal IDs */
enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint e_cal_view_signals[LAST_SIGNAL] = { 0 };

static void
e_cal_view_class_init (ECalViewClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* Create class' signals */
	e_cal_view_signals[SELECTION_CHANGED] =
		g_signal_new ("selection_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalViewClass, selection_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/* Method override */
	object_class->destroy = e_cal_view_destroy;

	klass->selection_changed = NULL;
}

static void
e_cal_view_init (ECalView *cal_view, ECalViewClass *klass)
{
	cal_view->priv = g_new0 (ECalViewPrivate, 1);
}

static void
e_cal_view_destroy (GtkObject *object)
{
	ECalView *cal_view = (ECalView *) object;

	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (cal_view->priv) {
		if (cal_view->priv->activity) {
			g_object_unref (cal_view->priv->activity);
			cal_view->priv->activity = NULL;
		}

		g_free (cal_view->priv);
		cal_view->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

E_MAKE_TYPE (e_cal_view, "ECalView", ECalView, e_cal_view_class_init,
	     e_cal_view_init, GTK_TYPE_TABLE);

GnomeCalendar *
e_cal_view_get_calendar (ECalView *cal_view)
{
	g_return_val_if_fail (E_IS_CAL_VIEW (cal_view), NULL);

	return cal_view->priv->calendar;
}

void
e_cal_view_set_calendar (ECalView *cal_view, GnomeCalendar *calendar)
{
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	cal_view->priv->calendar = calendar;
}

void
e_cal_view_set_status_message (ECalView *cal_view, const gchar *message)
{
	extern EvolutionShellClient *global_shell_client; /* ugly */

	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (!message || !*message) {
		if (cal_view->priv->activity) {
			g_object_unref (cal_view->priv->activity);
			cal_view->priv->activity = NULL;
		}
	} else if (!cal_view->priv->activity) {
		int display;
		char *client_id = g_strdup_printf ("%p", cal_view);

		if (progress_icon[0] == NULL)
			progress_icon[0] = gdk_pixbuf_new_from_file (EVOLUTION_IMAGESDIR "/" EVOLUTION_CALENDAR_PROGRESS_IMAGE, NULL);
		cal_view->priv->activity = evolution_activity_client_new (
			global_shell_client, client_id,
			progress_icon, message, TRUE, &display);

		g_free (client_id);
	} else
		evolution_activity_client_update (cal_view->priv->activity, message, -1.0);
}
