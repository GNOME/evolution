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
#include "e-cell-date-edit-config.h"
#include "e-calendar-table-config.h"

struct _ECalendarTableConfigPrivate {
	ECalendarTable *table;

	ECellDateEditConfig *cell_config;
	
	GList *notifications;
};

/* Property IDs */
enum props {
	PROP_0,
	PROP_TABLE
};

G_DEFINE_TYPE (ECalendarTableConfig, e_calendar_table_config, G_TYPE_OBJECT);

static void
e_calendar_table_config_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	ECalendarTableConfig *table_config;
	ECalendarTableConfigPrivate *priv;

	table_config = E_CALENDAR_TABLE_CONFIG (object);
	priv = table_config->priv;
	
	switch (property_id) {
	case PROP_TABLE:
		e_calendar_table_config_set_table (table_config, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_calendar_table_config_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	ECalendarTableConfig *table_config;
	ECalendarTableConfigPrivate *priv;

	table_config = E_CALENDAR_TABLE_CONFIG (object);
	priv = table_config->priv;
	
	switch (property_id) {
	case PROP_TABLE:
		g_value_set_object (value, e_calendar_table_config_get_table (table_config));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_calendar_table_config_dispose (GObject *object)
{
	ECalendarTableConfig *table_config = E_CALENDAR_TABLE_CONFIG (object);
	ECalendarTableConfigPrivate *priv;
	
	priv = table_config->priv;

	e_calendar_table_config_set_table (table_config, NULL);
	
	if (G_OBJECT_CLASS (e_calendar_table_config_parent_class)->dispose)
		G_OBJECT_CLASS (e_calendar_table_config_parent_class)->dispose (object);
}

static void
e_calendar_table_config_finalize (GObject *object)
{
	ECalendarTableConfig *table_config = E_CALENDAR_TABLE_CONFIG (object);
	ECalendarTableConfigPrivate *priv;
	
	priv = table_config->priv;

	g_free (priv);
	
	if (G_OBJECT_CLASS (e_calendar_table_config_parent_class)->finalize)
		G_OBJECT_CLASS (e_calendar_table_config_parent_class)->finalize (object);
}

static void
e_calendar_table_config_class_init (ECalendarTableConfigClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GParamSpec *spec;
	
	/* Method override */
	gobject_class->set_property = e_calendar_table_config_set_property;
	gobject_class->get_property = e_calendar_table_config_get_property;
	gobject_class->dispose = e_calendar_table_config_dispose;
	gobject_class->finalize = e_calendar_table_config_finalize;

	spec = g_param_spec_object ("table", NULL, NULL, e_calendar_table_get_type (),
				    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (gobject_class, PROP_TABLE, spec);
}

static void
e_calendar_table_config_init (ECalendarTableConfig *table_config)
{
	table_config->priv = g_new0 (ECalendarTableConfigPrivate, 1);

}

ECalendarTableConfig *
e_calendar_table_config_new (ECalendarTable *table)
{
	ECalendarTableConfig *table_config;
	
	table_config = g_object_new (e_calendar_table_config_get_type (), "table", table, NULL);

	return table_config;
}

ECalendarTable *
e_calendar_table_config_get_table (ECalendarTableConfig *table_config) 
{
	ECalendarTableConfigPrivate *priv;

	g_return_val_if_fail (table_config != NULL, NULL);
	g_return_val_if_fail (E_IS_CALENDAR_TABLE_CONFIG (table_config), NULL);

	priv = table_config->priv;
	
	return priv->table;
}

static void
set_timezone (ECalendarTable *table) 
{
	ECalModel *model;
	icaltimezone *zone;
	
	zone = calendar_config_get_icaltimezone ();	
	model = e_calendar_table_get_model (table);
	if (model)
		e_cal_model_set_timezone (model, zone);
}

static void
timezone_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	ECalendarTableConfig *table_config = data;
	ECalendarTableConfigPrivate *priv;
	
	priv = table_config->priv;
	
	set_timezone (priv->table);
}

static void
set_twentyfour_hour (ECalendarTable *table) 
{
	ECalModel *model;
	gboolean use_24_hour;

	use_24_hour = calendar_config_get_24_hour_format ();

	model = e_calendar_table_get_model (table);
	if (model)
		e_cal_model_set_use_24_hour_format (model, use_24_hour);
}

static void
twentyfour_hour_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	ECalendarTableConfig *table_config = data;
	ECalendarTableConfigPrivate *priv;
	
	priv = table_config->priv;
	
	set_twentyfour_hour (priv->table);
}

void
e_calendar_table_config_set_table (ECalendarTableConfig *table_config, ECalendarTable *table) 
{
	ECalendarTableConfigPrivate *priv;
	guint not;
	GList *l;
	
	g_return_if_fail (table_config != NULL);
	g_return_if_fail (E_IS_CALENDAR_TABLE_CONFIG (table_config));

	priv = table_config->priv;
	
	if (priv->table) {
		g_object_unref (priv->table);
		priv->table = NULL;
	}

	if (priv->cell_config) {
		g_object_unref (priv->cell_config);
		priv->cell_config = NULL;
	}
	
	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));

	g_list_free (priv->notifications);
	priv->notifications = NULL;

	/* If the new view is NULL, return right now */
	if (!table)
		return;
	
	priv->table = g_object_ref (table);

	/* Time zone */
	set_timezone (table);
	
	not = calendar_config_add_notification_timezone (timezone_changed_cb, table_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* 24 Hour format */
	set_twentyfour_hour (table);	

	not = calendar_config_add_notification_24_hour_format (twentyfour_hour_changed_cb, table_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Date cell */
	priv->cell_config = e_cell_date_edit_config_new (table->dates_cell);
}
