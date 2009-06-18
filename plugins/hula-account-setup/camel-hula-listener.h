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
 *		Harish Krishnaswamy <kharish@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef CAMEL_HULA_LISTENER_H
#define CAMEL_HULA_LISTENER_H

#include <libedataserver/e-account-list.h>
#include<libedataserver/e-source.h>
#include<libedataserver/e-source-list.h>
#include <camel/camel-url.h>

G_BEGIN_DECLS

#define CAMEL_TYPE_HULA_LISTENER            (camel_hula_listener_get_type ())
#define CAMEL_HULA_LISTENER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAMEL_TYPE_HULA_LISTENER, CamelHulaListener))
#define CAMEL_HULA_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAMEL_TYPE_HULA_LISTENER,  CamelHULAListenerClass))
#define CAMEL_IS_HULALISTENER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAMEL_TYPE_HULA_LISTENER))
#define CAMEL_IS_HULA_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), CAMEL_TYPE_HULA_LISTENER))

typedef struct _CamelHulaListener CamelHulaListener;
typedef struct _CamelHulaListenerClass CamelHulaListenerClass;
typedef struct _CamelHulaListenerPrivate CamelHulaListenerPrivate;

struct _CamelHulaListener {
       GObject parent;

       CamelHulaListenerPrivate *priv;
};

struct _CamelHulaListenerClass {
       GObjectClass parent_class;
};

GType              camel_hula_listener_get_type (void);
CamelHulaListener *camel_hula_listener_new (void);

G_END_DECLS

#endif
