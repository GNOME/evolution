/*
 * e-shell-settings.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/**
 * SECTION: e-shell-settings
 * @short_description: settings management
 * @include: shell/e-shell-settings.h
 **/

#ifndef E_SHELL_SETTINGS_H
#define E_SHELL_SETTINGS_H

#include <shell/e-shell-common.h>

/* Standard GObject macros */
#define E_TYPE_SHELL_SETTINGS \
	(e_shell_settings_get_type ())
#define E_SHELL_SETTINGS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_SETTINGS, EShellSettings))
#define E_SHELL_SETTINGS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_SETTINGS, EShellSettingsClass))
#define E_IS_SHELL_SETTINGS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_SETTINGS))
#define E_IS_SHELL_SETTINGS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL_SETTINGS))
#define E_SHELL_SETTINGS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_SETTINGS, EShellSettingsClass))

G_BEGIN_DECLS

typedef struct _EShellSettings EShellSettings;
typedef struct _EShellSettingsClass EShellSettingsClass;
typedef struct _EShellSettingsPrivate EShellSettingsPrivate;

/**
 * EShellSettings:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShellSettings {
	GObject parent;
	EShellSettingsPrivate *priv;
};

struct _EShellSettingsClass {
	GObjectClass parent_class;
};

GType		e_shell_settings_get_type	(void);
void		e_shell_settings_install_property
						(GParamSpec *pspec);
void		e_shell_settings_bind_to_gconf	(EShellSettings *shell_settings,
						 const gchar *property_name,
						 const gchar *gconf_key);

G_END_DECLS

#endif /* E_SHELL_SETTINGS_H */
