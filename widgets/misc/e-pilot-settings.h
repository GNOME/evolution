/*
 *
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
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_PILOT_SETTINGS_H_
#define _E_PILOT_SETTINGS_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libedataserver/e-source-list.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_PILOT_SETTINGS			(e_pilot_settings_get_type ())
#define E_PILOT_SETTINGS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_PILOT_SETTINGS, EPilotSettings))
#define E_PILOT_SETTINGS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_PILOT_SETTINGS, EPilotSettingsClass))
#define E_IS_PILOT_SETTINGS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_PILOT_SETTINGS))
#define E_IS_PILOT_SETTINGS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_PILOT_SETTINGS))


typedef struct _EPilotSettings        EPilotSettings;
typedef struct _EPilotSettingsPrivate EPilotSettingsPrivate;
typedef struct _EPilotSettingsClass   EPilotSettingsClass;

#define E_PILOT_SETTINGS_TABLE_ROWS 3
#define E_PILOT_SETTINGS_TABLE_COLS 3

struct _EPilotSettings {
	GtkTable parent;

	EPilotSettingsPrivate *priv;
};

struct _EPilotSettingsClass {
	GtkTableClass parent_class;
};


GType      e_pilot_settings_get_type (void);
GtkWidget *e_pilot_settings_new      (ESourceList *source_list);

ESource *e_pilot_settings_get_source (EPilotSettings *ps);
void e_pilot_settings_set_source (EPilotSettings *ps, ESource *source);

gboolean e_pilot_settings_get_secret (EPilotSettings *ps);
void e_pilot_settings_set_secret (EPilotSettings *ps, gboolean secret);

G_END_DECLS

#endif
