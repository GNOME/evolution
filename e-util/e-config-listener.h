/*
 * Configuration component listener
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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_CONFIG_LISTENER_H__
#define __E_CONFIG_LISTENER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define E_CONFIG_LISTENER_TYPE        (e_config_listener_get_type ())
#define E_CONFIG_LISTENER(o)          (G_TYPE_CHECK_INSTANCECAST ((o), E_CONFIG_LISTENER_TYPE, EConfigListener))
#define E_CONFIG_LISTENER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CONFIG_LISTENER_TYPE, EConfigListenerClass))
#define E_IS_CONFIG_LISTENER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CONFIG_LISTENER_TYPE))
#define E_IS_CONFIG_LISTENER_CLASS(k) (GT_TYPE_CHECK_CLASS_TYPE ((k), E_CONFIG_LISTENER_TYPE))

typedef struct _EConfigListenerPrivate EConfigListenerPrivate;

typedef struct {
	GObject object;
	EConfigListenerPrivate *priv;
} EConfigListener;

typedef struct {
	GObjectClass parent_class;

	void (* key_changed) (EConfigListener *cl, const gchar *key);
} EConfigListenerClass;

GType                 e_config_listener_get_type (void);
EConfigListener      *e_config_listener_new (void);

gboolean              e_config_listener_get_boolean (EConfigListener *cl, const gchar *key);
gboolean              e_config_listener_get_boolean_with_default (EConfigListener *cl,
								  const gchar *key,
								  gboolean def,
								  gboolean *used_default);
float                 e_config_listener_get_float (EConfigListener *cl, const gchar *key);
float                 e_config_listener_get_float_with_default (EConfigListener *cl,
								const gchar *key,
								float def,
								gboolean *used_default);
long                  e_config_listener_get_long (EConfigListener *cl, const gchar *key);
long                  e_config_listener_get_long_with_default (EConfigListener *cl,
							       const gchar *key,
							       long def,
							       gboolean *used_default);
gchar                 *e_config_listener_get_string (EConfigListener *cl, const gchar *key);
gchar                 *e_config_listener_get_string_with_default (EConfigListener *cl,
								 const gchar *key,
								 const gchar *def,
								 gboolean *used_default);
void                  e_config_listener_set_boolean (EConfigListener *cl,
						     const gchar *key,
						     gboolean value);
void                  e_config_listener_set_float (EConfigListener *cl,
						   const gchar *key,
						   float value);
void                  e_config_listener_set_long (EConfigListener *cl,
						  const gchar *key,
						  long value);
void                  e_config_listener_set_string (EConfigListener *cl,
						    const gchar *key,
						    const gchar *value);

void                  e_config_listener_remove_value (EConfigListener *cl,
						      const gchar *key);
void                  e_config_listener_remove_dir (EConfigListener *cl,
						    const gchar *dir);

G_END_DECLS

#endif
