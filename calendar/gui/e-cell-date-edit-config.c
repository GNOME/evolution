/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Author : 
 *  JP Rosevear <jpr@ximian.com>
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

#include "calendar-config.h"
#include "e-mini-calendar-config.h"
#include "e-cell-date-edit-config.h"

struct _ECellDateEditConfigPrivate {
	ECellDateEdit *cell;

	EMiniCalendarConfig *mini_config;
	
	GList *notifications;
};

/* Property IDs */
enum props {
	PROP_0,
	PROP_CELL,
};

G_DEFINE_TYPE (ECellDateEditConfig, e_cell_date_edit_config, G_TYPE_OBJECT);

static void
e_cell_date_edit_config_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	ECellDateEditConfig *view_config;
	ECellDateEditConfigPrivate *priv;

	view_config = E_CELL_DATE_EDIT_CONFIG (object);
	priv = view_config->priv;
	
	switch (property_id) {
	case PROP_CELL:
		e_cell_date_edit_config_set_cell (view_config, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_cell_date_edit_config_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	ECellDateEditConfig *view_config;
	ECellDateEditConfigPrivate *priv;

	view_config = E_CELL_DATE_EDIT_CONFIG (object);
	priv = view_config->priv;
	
	switch (property_id) {
	case PROP_CELL:
		g_value_set_object (value, e_cell_date_edit_config_get_cell (view_config));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_cell_date_edit_config_dispose (GObject *object)
{
	ECellDateEditConfig *view_config = E_CELL_DATE_EDIT_CONFIG (object);
	ECellDateEditConfigPrivate *priv;
	
	priv = view_config->priv;

	e_cell_date_edit_config_set_cell (view_config, NULL);
	
	if (G_OBJECT_CLASS (e_cell_date_edit_config_parent_class)->dispose)
		G_OBJECT_CLASS (e_cell_date_edit_config_parent_class)->dispose (object);
}

static void
e_cell_date_edit_config_finalize (GObject *object)
{
	ECellDateEditConfig *view_config = E_CELL_DATE_EDIT_CONFIG (object);
	ECellDateEditConfigPrivate *priv;
	
	priv = view_config->priv;

	g_free (priv);
	
	if (G_OBJECT_CLASS (e_cell_date_edit_config_parent_class)->finalize)
		G_OBJECT_CLASS (e_cell_date_edit_config_parent_class)->finalize (object);
}

static void
e_cell_date_edit_config_class_init (ECellDateEditConfigClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GParamSpec *spec;
	
	/* Method override */
	gobject_class->set_property = e_cell_date_edit_config_set_property;
	gobject_class->get_property = e_cell_date_edit_config_get_property;
	gobject_class->dispose = e_cell_date_edit_config_dispose;
	gobject_class->finalize = e_cell_date_edit_config_finalize;

	spec = g_param_spec_object ("cell", NULL, NULL, e_cell_date_edit_get_type (),
				    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (gobject_class, PROP_CELL, spec);
}

static void
e_cell_date_edit_config_init (ECellDateEditConfig *view_config)
{
	view_config->priv = g_new0 (ECellDateEditConfigPrivate, 1);

}

ECellDateEditConfig *
e_cell_date_edit_config_new (ECellDateEdit *cell)
{
	ECellDateEditConfig *view_config;
	
	view_config = g_object_new (e_cell_date_edit_config_get_type (), "cell", cell, NULL);

	return view_config;
}

ECellDateEdit *
e_cell_date_edit_config_get_cell (ECellDateEditConfig *view_config) 
{
	ECellDateEditConfigPrivate *priv;

	g_return_val_if_fail (view_config != NULL, NULL);
	g_return_val_if_fail (E_IS_CELL_DATE_EDIT_CONFIG (view_config), NULL);

	priv = view_config->priv;
	
	return priv->cell;
}

static void
set_timezone (ECellDateEdit *cell) 
{
	ECellDateEditText *cell_text;
	ECellPopup *cell_popup;
	icaltimezone *zone;
	
	zone = calendar_config_get_icaltimezone ();

	cell_popup = E_CELL_POPUP (cell);
	cell_text = E_CELL_DATE_EDIT_TEXT (cell_popup->child);
	e_cell_date_edit_text_set_timezone (cell_text, zone);
}

static void
timezone_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	ECellDateEditConfig *view_config = data;
	ECellDateEditConfigPrivate *priv;
	
	priv = view_config->priv;
	
	set_timezone (priv->cell);
}

static void
set_twentyfour_hour (ECellDateEdit *cell) 
{
	ECellDateEditText *cell_text;
	ECellPopup *cell_popup;
	gboolean use_24_hour;

	use_24_hour = calendar_config_get_24_hour_format ();

	e_cell_date_edit_freeze (cell);
	g_object_set (G_OBJECT (cell),
		      "use_24_hour_format", use_24_hour,
		      NULL);
	e_cell_date_edit_thaw (cell);

	cell_popup = E_CELL_POPUP (cell);
	cell_text = E_CELL_DATE_EDIT_TEXT (cell_popup->child);
	e_cell_date_edit_text_set_use_24_hour_format (cell_text, use_24_hour);
}

static void
twentyfour_hour_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	ECellDateEditConfig *view_config = data;
	ECellDateEditConfigPrivate *priv;
	
	priv = view_config->priv;
	
	set_twentyfour_hour (priv->cell);
}

static void
set_range (ECellDateEdit *cell) 
{
	int start_hour, end_hour;

	start_hour = calendar_config_get_day_start_hour ();
	end_hour = calendar_config_get_day_end_hour ();

	/* Round up the end hour. */
	if (calendar_config_get_day_end_minute () != 0)
		end_hour++;

	/* Make sure the start hour is ok */
	if (start_hour > end_hour)
		start_hour = end_hour;

      /* We use the default 0 - 24 now. */
#if 0	
	g_object_set (G_OBJECT (cell),
		      "lower_hour", start_hour,
		      "upper_hour", end_hour,
		      NULL);
#endif
}

static void
day_start_hour_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	ECellDateEditConfig *view_config = data;
	ECellDateEditConfigPrivate *priv;
	
	priv = view_config->priv;
	
	set_range (priv->cell);
}

static void
day_end_hour_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	ECellDateEditConfig *view_config = data;
	ECellDateEditConfigPrivate *priv;
	
	priv = view_config->priv;
	
	set_range (priv->cell);
}

static void
day_end_minute_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	ECellDateEditConfig *view_config = data;
	ECellDateEditConfigPrivate *priv;
	
	priv = view_config->priv;
	
	set_range (priv->cell);
}

void
e_cell_date_edit_config_set_cell (ECellDateEditConfig *view_config, ECellDateEdit *cell) 
{
	ECellDateEditConfigPrivate *priv;
	guint not;
	GList *l;
	
	g_return_if_fail (view_config != NULL);
	g_return_if_fail (E_IS_CELL_DATE_EDIT_CONFIG (view_config));

	priv = view_config->priv;
	
	if (priv->cell) {
		g_object_unref (priv->cell);
		priv->cell = NULL;
	}

	if (priv->mini_config) {
		g_object_unref (priv->mini_config);
		priv->mini_config = NULL;
	}
	
	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));

	g_list_free (priv->notifications);
	priv->notifications = NULL;

	/* If the new view is NULL, return right now */
	if (!cell)
		return;
	
	priv->cell = g_object_ref (cell);

	/* Time zone */
	set_timezone (cell);
	
	not = calendar_config_add_notification_timezone (timezone_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* 24 Hour format */
	set_twentyfour_hour (cell);	

	not = calendar_config_add_notification_24_hour_format (twentyfour_hour_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Popup time range */
	set_range (cell);

	not = calendar_config_add_notification_day_start_hour (day_start_hour_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
	not = calendar_config_add_notification_day_end_hour (day_end_hour_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
	not = calendar_config_add_notification_day_end_minute (day_end_minute_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* The mini calendar */
	priv->mini_config = e_mini_calendar_config_new (E_CALENDAR (cell->calendar));	
}
