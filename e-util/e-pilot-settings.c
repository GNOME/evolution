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

#include <gnome.h>
#include "e-pilot-settings.h"

struct _EPilotSettingsPrivate 
{
	GtkWidget *secret;
	GtkWidget *cat;
	GtkWidget *cat_btn;
};


static void class_init (EPilotSettingsClass *klass);
static void init (EPilotSettings *ps);

static GtkObjectClass *parent_class = NULL;


GtkType
e_pilot_settings_get_type (void)
{
  static GtkType type = 0;

  if (type == 0)
    {
      static const GtkTypeInfo info =
      {
        "EPilotSettings",
        sizeof (EPilotSettings),
        sizeof (EPilotSettingsClass),
        (GtkClassInitFunc) class_init,
        (GtkObjectInitFunc) init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      type = gtk_type_unique (gtk_table_get_type (), &info);
    }

  return type;
}

static void
class_init (EPilotSettingsClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (gtk_table_get_type ());

}


static void
init (EPilotSettings *ps)
{
	EPilotSettingsPrivate *priv;
	GtkWidget *lbl;
	
	priv = g_new0 (EPilotSettingsPrivate, 1);

	ps->priv = priv;

	gtk_table_resize (GTK_TABLE (ps), 2, 2);
	gtk_container_set_border_width (GTK_CONTAINER (ps), 4);
	gtk_table_set_col_spacings (GTK_TABLE (ps), 4);

	lbl = gtk_label_new (_("Sync Private Records:"));
	gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.5);
	priv->secret = gtk_check_button_new ();
	gtk_table_attach_defaults (GTK_TABLE (ps), lbl, 0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE (ps), priv->secret, 1, 2, 0, 1);
	gtk_widget_show (lbl);
	gtk_widget_show (priv->secret);

#if 0
	lbl = gtk_label_new (_("Sync Categories:"));
	gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.5);
	priv->cat = gtk_check_button_new ();
	gtk_table_attach_defaults (GTK_TABLE (ps), lbl, 0, 1, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE (ps), priv->cat, 1, 2, 1, 2);
	gtk_widget_show (lbl);
	gtk_widget_show (priv->cat);
#endif
}



GtkWidget *
e_pilot_settings_new (void)
{
	return gtk_type_new (E_TYPE_PILOT_SETTINGS);
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

