/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-pilot-settings.c
 *
 * Copyright (C) 2001  JP Rosevear
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 *
 * Author: JP Rosevear
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <libedataserverui/e-source-option-menu.h>
#include "e-pilot-settings.h"

struct _EPilotSettingsPrivate 
{
	GtkWidget *source;
	GtkWidget *secret;
	GtkWidget *cat;
	GtkWidget *cat_btn;
};


static void class_init (EPilotSettingsClass *klass);
static void init (EPilotSettings *ps);

static GObjectClass *parent_class = NULL;


GType
e_pilot_settings_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
                        sizeof (EPilotSettingsClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) class_init,
                        NULL, NULL,
                        sizeof (EPilotSettings),
                        0,
                        (GInstanceInitFunc) init
                };
		type = g_type_register_static (GTK_TYPE_TABLE, "EPilotSettings", &info, 0);
	}

	return type;
}

static void
class_init (EPilotSettingsClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (GTK_TYPE_TABLE);
}

static void
init (EPilotSettings *ps)
{
	EPilotSettingsPrivate *priv;
	
	priv = g_new0 (EPilotSettingsPrivate, 1);

	ps->priv = priv;
}


static void
build_ui (EPilotSettings *ps, ESourceList *source_list)
{
	EPilotSettingsPrivate *priv;
	GtkWidget *lbl;
	
	priv = ps->priv;

	gtk_table_resize (GTK_TABLE (ps), 2, 2);
	gtk_container_set_border_width (GTK_CONTAINER (ps), 4);
	gtk_table_set_col_spacings (GTK_TABLE (ps), 6);

	lbl = gtk_label_new (_("Sync with:"));
	gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.5);
	priv->source = e_source_option_menu_new (source_list);
	gtk_table_attach_defaults (GTK_TABLE (ps), lbl, 0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE (ps), priv->source, 1, 2, 0, 1);
	gtk_widget_show (lbl);
	gtk_widget_show (priv->source);

	lbl = gtk_label_new (_("Sync Private Records:"));
	gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.5);
	priv->secret = gtk_check_button_new ();
	gtk_table_attach_defaults (GTK_TABLE (ps), lbl, 0, 1, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE (ps), priv->secret, 1, 2, 1, 2);
	gtk_widget_show (lbl);
	gtk_widget_show (priv->secret);

#if 0
	lbl = gtk_label_new (_("Sync Categories:"));
	gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.5);
	priv->cat = gtk_check_button_new ();
	gtk_table_attach_defaults (GTK_TABLE (ps), lbl, 0, 1, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE (ps), priv->cat, 1, 2, 2, 3);
	gtk_widget_show (lbl);
	gtk_widget_show (priv->cat);
#endif
}



GtkWidget *
e_pilot_settings_new (ESourceList *source_list)
{
	EPilotSettings *ps;
	EPilotSettingsPrivate *priv;
	
	ps = g_object_new (E_TYPE_PILOT_SETTINGS, NULL);
	priv = ps->priv;

	build_ui (ps, source_list);
	
	return GTK_WIDGET (ps);
}

ESource *
e_pilot_settings_get_source (EPilotSettings *ps)
{
	EPilotSettingsPrivate *priv;
	
	g_return_val_if_fail (ps != NULL, FALSE);
	g_return_val_if_fail (E_IS_PILOT_SETTINGS (ps), FALSE);

	priv = ps->priv;
	
	return e_source_option_menu_peek_selected (E_SOURCE_OPTION_MENU (priv->source));
}

void
e_pilot_settings_set_source (EPilotSettings *ps, ESource *source)
{
	EPilotSettingsPrivate *priv;
	
	g_return_if_fail (ps != NULL);
	g_return_if_fail (E_IS_PILOT_SETTINGS (ps));

	priv = ps->priv;

	e_source_option_menu_select (E_SOURCE_OPTION_MENU (priv->source), source);
}

gboolean
e_pilot_settings_get_secret (EPilotSettings *ps)
{
	EPilotSettingsPrivate *priv;
	
	g_return_val_if_fail (ps != NULL, FALSE);
	g_return_val_if_fail (E_IS_PILOT_SETTINGS (ps), FALSE);

	priv = ps->priv;
	
	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->secret));
}

void
e_pilot_settings_set_secret (EPilotSettings *ps, gboolean secret)
{
	EPilotSettingsPrivate *priv;
	
	g_return_if_fail (ps != NULL);
	g_return_if_fail (E_IS_PILOT_SETTINGS (ps));

	priv = ps->priv;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->secret),
				      secret);
}

