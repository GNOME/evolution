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

#include "calendar-config.h"
#include "e-cal-list-view-config.h"

struct _ECalListViewConfigPrivate {
	ECalListView *view;

	GList *notifications;
};

/* Property IDs */
enum props {
	PROP_0,
	PROP_VIEW,
};

G_DEFINE_TYPE (ECalListViewConfig, e_cal_list_view_config, G_TYPE_OBJECT);

static void
e_cal_list_view_config_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	ECalListViewConfig *view_config;
	ECalListViewConfigPrivate *priv;

	view_config = E_CAL_LIST_VIEW_CONFIG (object);
	priv = view_config->priv;
	
	switch (property_id) {
	case PROP_VIEW:
		e_cal_list_view_config_set_view (view_config, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_cal_list_view_config_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	ECalListViewConfig *view_config;
	ECalListViewConfigPrivate *priv;

	view_config = E_CAL_LIST_VIEW_CONFIG (object);
	priv = view_config->priv;
	
	switch (property_id) {
	case PROP_VIEW:
		g_value_set_object (value, e_cal_list_view_config_get_view (view_config));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_cal_list_view_config_dispose (GObject *object)
{
	ECalListViewConfig *view_config = E_CAL_LIST_VIEW_CONFIG (object);
	ECalListViewConfigPrivate *priv;
	
	priv = view_config->priv;

	e_cal_list_view_config_set_view (view_config, NULL);
	
	if (G_OBJECT_CLASS (e_cal_list_view_config_parent_class)->dispose)
		G_OBJECT_CLASS (e_cal_list_view_config_parent_class)->dispose (object);
}

static void
e_cal_list_view_config_finalize (GObject *object)
{
	ECalListViewConfig *view_config = E_CAL_LIST_VIEW_CONFIG (object);
	ECalListViewConfigPrivate *priv;
	
	priv = view_config->priv;

	g_free (priv);
	
	if (G_OBJECT_CLASS (e_cal_list_view_config_parent_class)->finalize)
		G_OBJECT_CLASS (e_cal_list_view_config_parent_class)->finalize (object);
}

static void
e_cal_list_view_config_class_init (ECalListViewConfigClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GParamSpec *spec;
	
	/* Method override */
	gobject_class->set_property = e_cal_list_view_config_set_property;
	gobject_class->get_property = e_cal_list_view_config_get_property;
	gobject_class->dispose = e_cal_list_view_config_dispose;
	gobject_class->finalize = e_cal_list_view_config_finalize;

	spec = g_param_spec_object ("view", NULL, NULL, e_cal_list_view_get_type (),
				    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (gobject_class, PROP_VIEW, spec);
}

static void
e_cal_list_view_config_init (ECalListViewConfig *view_config)
{
	view_config->priv = g_new0 (ECalListViewConfigPrivate, 1);

}

ECalListViewConfig *
e_cal_list_view_config_new (ECalListView *list_view)
{
	ECalListViewConfig *view_config;
	
	view_config = g_object_new (e_cal_list_view_config_get_type (), "view", list_view, NULL);

	return view_config;
}

ECalListView *
e_cal_list_view_config_get_view (ECalListViewConfig *view_config) 
{
	ECalListViewConfigPrivate *priv;

	g_return_val_if_fail (view_config != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_LIST_VIEW_CONFIG (view_config), NULL);

	priv = view_config->priv;
	
	return priv->view;
}

static void
set_timezone (ECalListView *list_view) 
{
	icaltimezone *zone;
	
	zone = calendar_config_get_icaltimezone ();	
	e_calendar_view_set_timezone (E_CALENDAR_VIEW (list_view), zone);
}

static void
timezone_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	ECalListViewConfig *view_config = data;
	ECalListViewConfigPrivate *priv;
	
	priv = view_config->priv;
	
	set_timezone (priv->view);
}

static void
set_twentyfour_hour (ECalListView *list_view) 
{
	gboolean use_24_hour;

	use_24_hour = calendar_config_get_24_hour_format ();

	e_calendar_view_set_use_24_hour_format (E_CALENDAR_VIEW (list_view), use_24_hour);
}

static void
twentyfour_hour_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	ECalListViewConfig *view_config = data;
	ECalListViewConfigPrivate *priv;
	
	priv = view_config->priv;
	
	set_twentyfour_hour (priv->view);
}

void
e_cal_list_view_config_set_view (ECalListViewConfig *view_config, ECalListView *list_view) 
{
	ECalListViewConfigPrivate *priv;
	guint not;
	GList *l;
	
	g_return_if_fail (view_config != NULL);
	g_return_if_fail (E_IS_CAL_LIST_VIEW_CONFIG (view_config));

	priv = view_config->priv;
	
	if (priv->view) {
		g_object_unref (priv->view);
		priv->view = NULL;
	}
	
	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));

	g_list_free (priv->notifications);
	priv->notifications = NULL;

	/* If the new view is NULL, return right now */
	if (!list_view)
		return;
	
	priv->view = g_object_ref (list_view);

	/* Time zone */
	set_timezone (list_view);
	
	not = calendar_config_add_notification_timezone (timezone_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* 24 Hour format */
	set_twentyfour_hour (list_view);	

	not = calendar_config_add_notification_24_hour_format (twentyfour_hour_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
}
