/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "calendar-config.h"
#include "e-cell-date-edit-config.h"
#include "e-calendar-table-config.h"

#define E_CALENDAR_TABLE_CONFIG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CALENDAR_TABLE_CONFIG, ECalendarTableConfigPrivate))

struct _ECalendarTableConfigPrivate {
	ECalendarTable *table;
	ECellDateEditConfig *cell_config;
	GList *notifications;
};

enum {
	PROP_0,
	PROP_TABLE
};

static gpointer parent_class;

static void
calendar_table_config_set_timezone (ECalendarTable *table)
{
	ECalModel *model;
	icaltimezone *zone;

	zone = calendar_config_get_icaltimezone ();
	model = e_calendar_table_get_model (table);
	if (model != NULL)
		e_cal_model_set_timezone (model, zone);
}

static void
calendar_table_config_timezone_changed_cb (GConfClient *client,
                                           guint id,
                                           GConfEntry *entry,
                                           gpointer data)
{
	ECalendarTableConfig *table_config = data;

	calendar_table_config_set_timezone (table_config->priv->table);
}

static void
calendar_table_config_set_twentyfour_hour (ECalendarTable *table)
{
	ECalModel *model;
	gboolean use_24_hour;

	use_24_hour = calendar_config_get_24_hour_format ();

	model = e_calendar_table_get_model (table);
	if (model != NULL)
		e_cal_model_set_use_24_hour_format (model, use_24_hour);
}

static void
calendar_table_config_twentyfour_hour_changed_cb (GConfClient *client,
                                                  guint id,
                                                  GConfEntry *entry,
                                                  gpointer data)
{
	ECalendarTableConfig *table_config = data;

	calendar_table_config_set_twentyfour_hour (table_config->priv->table);
}

static void
calendar_table_config_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TABLE:
			e_calendar_table_config_set_table (
				E_CALENDAR_TABLE_CONFIG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
calendar_table_config_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TABLE:
			g_value_set_object (
				value, e_calendar_table_config_get_table (
				E_CALENDAR_TABLE_CONFIG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
calendar_table_config_dispose (GObject *object)
{
	ECalendarTableConfig *table_config = E_CALENDAR_TABLE_CONFIG (object);

	e_calendar_table_config_set_table (table_config, NULL);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
calendar_table_config_class_init (ECalendarTableConfigClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalendarTableConfigPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = calendar_table_config_set_property;
	object_class->get_property = calendar_table_config_get_property;
	object_class->dispose = calendar_table_config_dispose;

	g_object_class_install_property (
		object_class,
		PROP_TABLE,
		g_param_spec_object (
			"table",
			NULL,
			NULL,
			E_TYPE_CALENDAR_TABLE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
calendar_table_config_init (ECalendarTableConfig *table_config)
{
	table_config->priv =
		E_CALENDAR_TABLE_CONFIG_GET_PRIVATE (table_config);
}

GType
e_calendar_table_config_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ECalendarTableConfigClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) calendar_table_config_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ECalendarTableConfig),
			0,     /* n_preallocs */
			(GInstanceInitFunc) calendar_table_config_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "ECalendarTableConfig", &type_info, 0);
	}

	return type;
}

ECalendarTableConfig *
e_calendar_table_config_new (ECalendarTable *table)
{
	g_return_val_if_fail (E_IS_CALENDAR_TABLE (table), NULL);

	return g_object_new (
		E_TYPE_CALENDAR_TABLE_CONFIG,
		"table", table, NULL);
}

ECalendarTable *
e_calendar_table_config_get_table (ECalendarTableConfig *table_config)
{
	g_return_val_if_fail (E_IS_CALENDAR_TABLE_CONFIG (table_config), NULL);

	return table_config->priv->table;
}

void
e_calendar_table_config_set_table (ECalendarTableConfig *table_config,
                                   ECalendarTable *table)
{
	ECalendarTableConfigPrivate *priv;
	guint notification;
	GList *list, *iter;

	g_return_if_fail (E_IS_CALENDAR_TABLE_CONFIG (table_config));

	priv = table_config->priv;

	if (table_config->priv->table) {
		g_object_unref (table_config->priv->table);
		table_config->priv->table = NULL;
	}

	if (table_config->priv->cell_config) {
		g_object_unref (table_config->priv->cell_config);
		table_config->priv->cell_config = NULL;
	}

	list = table_config->priv->notifications;
	for (iter = list; iter != NULL; iter = iter->next) {
		notification = GPOINTER_TO_UINT (iter->data);
		calendar_config_remove_notification (notification);
	}
	g_list_free (list);
	table_config->priv->notifications = NULL;

	if (table == NULL)
		return;

	table_config->priv->table = g_object_ref (table);

	/* Time zone */
	calendar_table_config_set_timezone (table);

	notification = calendar_config_add_notification_timezone (
		calendar_table_config_timezone_changed_cb, table_config);
	table_config->priv->notifications = g_list_prepend (
		table_config->priv->notifications,
		GUINT_TO_POINTER (notification));

	/* 24 Hour format */
	calendar_table_config_set_twentyfour_hour (table);

	notification = calendar_config_add_notification_24_hour_format (
		calendar_table_config_twentyfour_hour_changed_cb, table_config);
	table_config->priv->notifications = g_list_prepend (
		table_config->priv->notifications,
		GUINT_TO_POINTER (notification));

	/* Date cell */
	table_config->priv->cell_config =
		e_cell_date_edit_config_new (table->dates_cell);
}
