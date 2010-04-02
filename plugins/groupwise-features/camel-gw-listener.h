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
 *		Sivaiah Nallagatla <snallagatla@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef CAMEL_GW_LISTENER_H
#define CAMEL_GW_LISTENER_H

#include <libedataserver/e-account-list.h>
#include<libedataserver/e-source.h>
#include<libedataserver/e-source-list.h>

G_BEGIN_DECLS

#define CAMEL_TYPE_GW_LISTENER            (camel_gw_listener_get_type ())
#define CAMEL_GW_LISTENER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAMEL_TYPE_GW_LISTENER, CamelGwListener))
#define CAMEL_GW_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAMEL_TYPE_GW_LISTENER,  CamelGWListenerClass))
#define CAMEL_IS_GWLISTENER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAMEL_TYPE_GW_LISTENER))
#define CAMEL_IS_GW_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), CAMEL_TYPE_GW_LISTENER))

typedef struct _CamelGwListener CamelGwListener;
typedef struct _CamelGwListenerClass CamelGwListenerClass;
typedef struct _CamelGwListenerPrivate CamelGwListenerPrivate;

struct _CamelGwListener {
       GObject parent;

       CamelGwListenerPrivate *priv;
};

struct _CamelGwListenerClass {
       GObjectClass parent_class;
};

GType                   camel_gw_listener_get_type (void);
CamelGwListener *camel_gw_listener_new (void);

G_END_DECLS

#endif
