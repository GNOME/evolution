/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors :
 *  
 *  Sivaiah Nallagatla <snallagatla@novell.com>
 *
 * Copyright 2003, Novell, Inc.
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

                                                                                                                             
#ifndef GROUPWISE_CONFIG_LISTENER_H
#define GROUPWISE_CONFIG_LISTENER_H
                                                                                                                             

#include <e-util/e-account-list.h>
#include<libedataserver/e-source.h>
#include<libedataserver/e-source-list.h>
#include<e-util/e-url.h>
                         
G_BEGIN_DECLS
                                                                                                                             
#define GROUPWISE_TYPE_CONFIG_LISTENER            (groupwise_config_listener_get_type ())
#define GROUPWISE_CONFIG_LISTENER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GROUPWISE_TYPE_CONFIG_LISTENER, GroupwiseConfigListener))
#define GROUPWISE_CONFIG_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GROUPWISE_TYPE_CONFIG_LISTENER, GroupwiseConfigListenerClass))
#define GROUPWISE_IS_CONFIG_LISTENER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GROUPWISE_TYPE_CONFIG_LISTENER))
#define GROUPWISE_IS_CONFIG_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), GROUPWISE_TYPE_CONFIG_LISTENER))

typedef struct _GroupwiseConfigListener GroupwiseConfigListener;
typedef struct _GroupwiseConfigListenerClass GroupwiseConfigListenerClass;
typedef struct _GroupwiseConfigListenerPrivate GroupwiseConfigListenerPrivate;
struct _GroupwiseConfigListener {
       GObject parent;
                                                                                                                        
       GroupwiseConfigListenerPrivate *priv;
};
                                                                                                                             
struct _GroupwiseConfigListenerClass {
       GObjectClass parent_class;
                                                                                                                             
       
};
                                                                                                                             
GType                   groupwise_config_listener_get_type (void);
GroupwiseConfigListener *groupwise_config_listener_new      (GConfClient *gconf);
                                                                                                                             
G_END_DECLS
                                                                                                                             
#endif 

