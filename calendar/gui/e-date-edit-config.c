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
#include "e-date-edit-config.h"

struct _EDateEditConfigPrivate {
	EDateEdit *edit;

	GList *notifications;
};

/* Property IDs */
enum props {
	PROP_0,
	PROP_EDIT,
};

G_DEFINE_TYPE (EDateEditConfig, e_date_edit_config, G_TYPE_OBJECT);

static void
e_date_edit_config_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	EDateEditConfig *edit_config;
	EDateEditConfigPrivate *priv;

	edit_config = E_DATE_EDIT_CONFIG (object);
	priv = edit_config->priv;
	
	switch (property_id) {
	case PROP_EDIT:
		e_date_edit_config_set_edit (edit_config, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_date_edit_config_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	EDateEditConfig *edit_config;
	EDateEditConfigPrivate *priv;

	edit_config = E_DATE_EDIT_CONFIG (object);
	priv = edit_config->priv;
	
	switch (property_id) {
	case PROP_EDIT:
		g_value_set_object (value, e_date_edit_config_get_edit (edit_config));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_date_edit_config_dispose (GObject *object)
{
	EDateEditConfig *edit_config = E_DATE_EDIT_CONFIG (object);
	EDateEditConfigPrivate *priv;
	
	priv = edit_config->priv;

	e_date_edit_config_set_edit (edit_config, NULL);
	
	if (G_OBJECT_CLASS (e_date_edit_config_parent_class)->dispose)
		G_OBJECT_CLASS (e_date_edit_config_parent_class)->dispose (object);
}

static void
e_date_edit_config_finalize (GObject *object)
{
	EDateEditConfig *edit_config = E_DATE_EDIT_CONFIG (object);
	EDateEditConfigPrivate *priv;
	
	priv = edit_config->priv;

	g_free (priv);
	
	if (G_OBJECT_CLASS (e_date_edit_config_parent_class)->finalize)
		G_OBJECT_CLASS (e_date_edit_config_parent_class)->finalize (object);
}

static void
e_date_edit_config_class_init (EDateEditConfigClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GParamSpec *spec;
	
	/* Method override */
	gobject_class->set_property = e_date_edit_config_set_property;
	gobject_class->get_property = e_date_edit_config_get_property;
	gobject_class->dispose = e_date_edit_config_dispose;
	gobject_class->finalize = e_date_edit_config_finalize;

	spec = g_param_spec_object ("edit", NULL, NULL, e_date_edit_get_type (),
				    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (gobject_class, PROP_EDIT, spec);
}

static void
e_date_edit_config_init (EDateEditConfig *edit_config)
{
	edit_config->priv = g_new0 (EDateEditConfigPrivate, 1);

}

EDateEditConfig *
e_date_edit_config_new (EDateEdit *date_edit)
{
	EDateEditConfig *edit_config;
	
	edit_config = g_object_new (e_date_edit_config_get_type (), "edit", date_edit, NULL);

	return edit_config;
}

EDateEdit *
e_date_edit_config_get_edit (EDateEditConfig *edit_config) 
{
	EDateEditConfigPrivate *priv;

	g_return_val_if_fail (edit_config != NULL, NULL);
	g_return_val_if_fail (E_IS_DATE_EDIT_CONFIG (edit_config), NULL);

	priv = edit_config->priv;
	
	return priv->edit;
}

static void
set_week_start (EDateEdit *date_edit) 
{
	int week_start_day;	

	week_start_day = calendar_config_get_week_start_day ();

	/* Convert it to 0 (Mon) to 6 (Sun), which is what we use. */
	week_start_day = (week_start_day + 6) % 7;

	e_date_edit_set_week_start_day (date_edit, week_start_day);
}

static void
week_start_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDateEditConfig *edit_config = data;
	EDateEditConfigPrivate *priv;
	
	priv = edit_config->priv;
	
	set_week_start (priv->edit);
}

static void
set_twentyfour_hour (EDateEdit *date_edit) 
{
	gboolean use_24_hour;

	use_24_hour = calendar_config_get_24_hour_format ();

	e_date_edit_set_use_24_hour_format (date_edit, use_24_hour);
}

static void
twentyfour_hour_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDateEditConfig *edit_config = data;
	EDateEditConfigPrivate *priv;
	
	priv = edit_config->priv;
	
	set_twentyfour_hour (priv->edit);
}

static void
set_dnav_show_week_no (EDateEdit *date_edit) 
{
	gboolean show_week_no;

	show_week_no = calendar_config_get_dnav_show_week_no ();

	e_date_edit_set_show_week_numbers (date_edit, show_week_no);
}

static void
dnav_show_week_no_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDateEditConfig *edit_config = data;
	EDateEditConfigPrivate *priv;
	
	priv = edit_config->priv;
	
	set_dnav_show_week_no (priv->edit);
}
void
e_date_edit_config_set_edit (EDateEditConfig *edit_config, EDateEdit *date_edit) 
{
	EDateEditConfigPrivate *priv;
	guint not;
	GList *l;
	
	g_return_if_fail (edit_config != NULL);
	g_return_if_fail (E_IS_DATE_EDIT_CONFIG (edit_config));

	priv = edit_config->priv;
	
	if (priv->edit) {
		g_object_unref (priv->edit);
		priv->edit = NULL;
	}
	
	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));

	g_list_free (priv->notifications);
	priv->notifications = NULL;

	/* If the new edit is NULL, return right now */
	if (!date_edit)
		return;
	
	priv->edit = g_object_ref (date_edit);

	/* Week start */
	set_week_start (date_edit);	

	not = calendar_config_add_notification_week_start_day (week_start_changed_cb, edit_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* 24 Hour format */
	set_twentyfour_hour (date_edit);	

	not = calendar_config_add_notification_24_hour_format (twentyfour_hour_changed_cb, edit_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Show week numbers */
	set_dnav_show_week_no (date_edit);

	not = calendar_config_add_notification_dnav_show_week_no (dnav_show_week_no_changed_cb, edit_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
}
