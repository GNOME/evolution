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

struct _EMiniCalendarConfigPrivate {
	ECalendar *mini_cal;

	GList *notifications;
};

/* Property IDs */
enum props {
	PROP_0,
	PROP_CALENDAR
};

G_DEFINE_TYPE (EMiniCalendarConfig, e_mini_calendar_config, G_TYPE_OBJECT);

static void
e_mini_calendar_config_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	EMiniCalendarConfig *mini_config;
	EMiniCalendarConfigPrivate *priv;

	mini_config = E_MINI_CALENDAR_CONFIG (object);
	priv = mini_config->priv;
	
	switch (property_id) {
	case PROP_CALENDAR:
		e_mini_calendar_config_set_calendar (mini_config, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_mini_calendar_config_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	EMiniCalendarConfig *mini_config;
	EMiniCalendarConfigPrivate *priv;

	mini_config = E_MINI_CALENDAR_CONFIG (object);
	priv = mini_config->priv;
	
	switch (property_id) {
	case PROP_CALENDAR:
		g_value_set_object (value, e_mini_calendar_config_get_calendar (mini_config));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_mini_calendar_config_dispose (GObject *object)
{
	EMiniCalendarConfig *mini_config = E_MINI_CALENDAR_CONFIG (object);
	EMiniCalendarConfigPrivate *priv;
	
	priv = mini_config->priv;

	e_mini_calendar_config_set_calendar (mini_config, NULL);
	
	if (G_OBJECT_CLASS (e_mini_calendar_config_parent_class)->dispose)
		G_OBJECT_CLASS (e_mini_calendar_config_parent_class)->dispose (object);
}

static void
e_mini_calendar_config_finalize (GObject *object)
{
	EMiniCalendarConfig *mini_config = E_MINI_CALENDAR_CONFIG (object);
	EMiniCalendarConfigPrivate *priv;
	
	priv = mini_config->priv;

	g_free (priv);
	
	if (G_OBJECT_CLASS (e_mini_calendar_config_parent_class)->finalize)
		G_OBJECT_CLASS (e_mini_calendar_config_parent_class)->finalize (object);
}

static void
e_mini_calendar_config_class_init (EMiniCalendarConfigClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GParamSpec *spec;
	
	/* Method override */
	gobject_class->set_property = e_mini_calendar_config_set_property;
	gobject_class->get_property = e_mini_calendar_config_get_property;
	gobject_class->dispose = e_mini_calendar_config_dispose;
	gobject_class->finalize = e_mini_calendar_config_finalize;

	spec = g_param_spec_object ("calendar", NULL, NULL, e_calendar_get_type (),
				    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (gobject_class, PROP_CALENDAR, spec);
}

static void
e_mini_calendar_config_init (EMiniCalendarConfig *mini_config)
{
	mini_config->priv = g_new0 (EMiniCalendarConfigPrivate, 1);

}

EMiniCalendarConfig *
e_mini_calendar_config_new (ECalendar *mini_cal)
{
	EMiniCalendarConfig *mini_config;
	
	mini_config = g_object_new (e_mini_calendar_config_get_type (), "calendar", mini_cal, NULL);

	return mini_config;
}

ECalendar *
e_mini_calendar_config_get_calendar (EMiniCalendarConfig *mini_config) 
{
	EMiniCalendarConfigPrivate *priv;

	g_return_val_if_fail (mini_config != NULL, NULL);
	g_return_val_if_fail (E_IS_MINI_CALENDAR_CONFIG (mini_config), NULL);

	priv = mini_config->priv;
	
	return priv->mini_cal;
}

static void
set_week_start (ECalendar *mini_cal) 
{
	int week_start_day;	

	week_start_day = calendar_config_get_week_start_day ();

	/* Convert it to 0 (Mon) to 6 (Sun), which is what we use. */
	week_start_day = (week_start_day + 6) % 7;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mini_cal->calitem),
			       "week_start_day", week_start_day,
			       NULL);
}

static void
week_start_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EMiniCalendarConfig *mini_config = data;
	EMiniCalendarConfigPrivate *priv;
	
	priv = mini_config->priv;
	
	set_week_start (priv->mini_cal);
}

static void
set_dnav_show_week_no (ECalendar *mini_cal) 
{
	gboolean show_week_no;

	show_week_no = calendar_config_get_dnav_show_week_no ();

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (mini_cal->calitem),
			       "show_week_numbers", show_week_no,
			       NULL);
}

static void
dnav_show_week_no_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EMiniCalendarConfig *mini_config = data;
	EMiniCalendarConfigPrivate *priv;
	
	priv = mini_config->priv;
	
	set_dnav_show_week_no (priv->mini_cal);
}

void
e_mini_calendar_config_set_calendar (EMiniCalendarConfig *mini_config, ECalendar *mini_cal) 
{
	EMiniCalendarConfigPrivate *priv;
	guint not;
	GList *l;
	
	g_return_if_fail (mini_config != NULL);
	g_return_if_fail (E_IS_MINI_CALENDAR_CONFIG (mini_config));

	priv = mini_config->priv;
	
	if (priv->mini_cal) {
		g_object_unref (priv->mini_cal);
		priv->mini_cal = NULL;
	}
	
	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));

	g_list_free (priv->notifications);
	priv->notifications = NULL;

	/* If the new view is NULL, return right now */
	if (!mini_cal)
		return;
	
	priv->mini_cal = g_object_ref (mini_cal);

	/* Week start */
	set_week_start (mini_cal);

	not = calendar_config_add_notification_week_start_day (week_start_changed_cb, mini_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Show week numbers */
	set_dnav_show_week_no (mini_cal);

	not = calendar_config_add_notification_dnav_show_week_no (dnav_show_week_no_changed_cb, mini_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
}
