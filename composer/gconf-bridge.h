/* 
 * (C) 2005 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GCONF_BRIDGE_H__
#define __GCONF_BRIDGE_H__

#include <gconf/gconf-client.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkliststore.h>

G_BEGIN_DECLS

void gconf_bridge_install_default_error_handler (void);

typedef struct _GConfBridge GConfBridge;

GConfBridge *gconf_bridge_get                    (void);

GConfClient *gconf_bridge_get_client             (GConfBridge  *bridge);

guint        gconf_bridge_bind_property_full     (GConfBridge  *bridge,
                                                  const char   *key,
                                                  GObject      *object,
                                                  const char   *prop,
                                                  gboolean      delayed_sync);

/**
 * gconf_bridge_bind_property
 * @bridge: A #GConfBridge
 * @key: A GConf key to be bound
 * @object: A #GObject
 * @prop: The property of @object to be bound
 *
 * Binds @key to @prop without delays, causing them to have the same value at all times. See
 * #gconf_bridge_bind_property_full for more details.
 *
 **/
#define gconf_bridge_bind_property(bridge, key, object, prop) \
        gconf_bridge_bind_property_full ((bridge), (key), \
                                         (object), (prop), FALSE)

/**
 * gconf_bridge_bind_property_delayed
 * @bridge: A #GConfBridge
 * @key: A GConf key to be bound
 * @object: A #GObject
 * @prop: The property of @object to be bound
 *
 * Binds @key to @prop with a delay, causing them to have the same value at all
 * times. See #gconf_bridge_bind_property_full for more details.
 **/
#define gconf_bridge_bind_property_delayed(bridge, key, object, prop) \
        gconf_bridge_bind_property_full ((bridge), (key), \
                                         (object), (prop), TRUE)

guint        gconf_bridge_bind_window            (GConfBridge  *bridge,
                                                  const char   *key_prefix,
                                                  GtkWindow    *window,
                                                  gboolean      bind_size,
                                                  gboolean      bind_pos);

/**
 * gconf_bridge_bind_window_size
 * @bridge: A #GConfBridge
 * @key_prefix: The prefix of the GConf keys
 * @window: A #GtkWindow
 * 
 * On calling this function @window will be resized to the values specified by
 * "@key_prefix<!-- -->_width" and "@key_prefix<!-- -->_height".  The respective
 * GConf values will be updated when the window is resized. See
 * #gconf_bridge_bind_window for more details.
 **/
#define gconf_bridge_bind_window_size(bridge, key_prefix, window) \
        gconf_bridge_bind_window ((bridge), (key_prefix), (window), TRUE, FALSE)

/**
 * gconf_bridge_bind_window_pos
 * @bridge: A #GConfBridge
 * @key_prefix: The prefix of the GConf keys
 * @window: A #GtkWindow
 * 
 * On calling this function @window will be moved to the values specified by
 * "@key_prefix<!-- -->_x" and "@key_prefix<!-- -->_y". The respective GConf
 * values will be updated when the window is moved. See
 * #gconf_bridge_bind_window for more details.
 **/
#define gconf_bridge_bind_window_pos(bridge, key_prefix, window) \
        gconf_bridge_bind_window ((bridge), (key_prefix), (window), FALSE, TRUE)

guint        gconf_bridge_bind_string_list_store (GConfBridge  *bridge,
                                                  const char   *key,
                                                  GtkListStore *list_store);

void         gconf_bridge_unbind                 (GConfBridge  *bridge,
                                                  guint         binding_id);

G_END_DECLS

#endif /* __GCONF_BRIDGE_H__ */
