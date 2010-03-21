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
void		e_shell_settings_install_property_for_key
						(const gchar *property_name,
						 const gchar *gconf_key);
void		e_shell_settings_enable_debug	(EShellSettings *shell_settings);

/* Getters and setters for common EShellSettings property types.
 * These are more convenient than g_object_get() / g_object_set().
 * Add more types as needed.  If GObject ever adds similar functions,
 * kill these. */

gboolean	e_shell_settings_get_boolean	(EShellSettings *shell_settings,
						 const gchar *property_name);
void		e_shell_settings_set_boolean	(EShellSettings *shell_settings,
						 const gchar *property_name,
						 gboolean v_boolean);
gint		e_shell_settings_get_int	(EShellSettings *shell_settings,
						 const gchar *property_name);
void		e_shell_settings_set_int	(EShellSettings *shell_settings,
						 const gchar *property_name,
						 gint v_int);
gchar *		e_shell_settings_get_string	(EShellSettings *shell_settings,
						 const gchar *property_name);
void		e_shell_settings_set_string	(EShellSettings *shell_settings,
						 const gchar *property_name,
						 const gchar *v_string);
gpointer	e_shell_settings_get_object	(EShellSettings *shell_settings,
						 const gchar *property_name);
void		e_shell_settings_set_object	(EShellSettings *shell_settings,
						 const gchar *property_name,
						 gpointer v_object);
gpointer	e_shell_settings_get_pointer	(EShellSettings *shell_settings,
						 const gchar *property_name);
void		e_shell_settings_set_pointer	(EShellSettings *shell_settings,
						 const gchar *property_name,
						 gpointer v_pointer);

G_END_DECLS

#endif /* E_SHELL_SETTINGS_H */
