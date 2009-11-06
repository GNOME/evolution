/*
 * e-activity-proxy.h
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

#ifndef E_ACTIVITY_PROXY_H
#define E_ACTIVITY_PROXY_H

#include <gtk/gtk.h>
#include <e-util/e-activity.h>

/* Standard GObject macros */
#define E_TYPE_ACTIVITY_PROXY \
	(e_activity_proxy_get_type ())
#define E_ACTIVITY_PROXY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ACTIVITY_PROXY, EActivityProxy))
#define E_ACTIVITY_PROXY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ACTIVITY_PROXY, EActivityProxyClass))
#define E_IS_ACTIVITY_PROXY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ACTIVITY_PROXY))
#define E_IS_ACTIVITY_PROXY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ACTIVITY_PROXY))
#define E_ACTIVITY_PROXY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ACTIVITY_PROXY, EActivityProxyClass))

G_BEGIN_DECLS

typedef struct _EActivityProxy EActivityProxy;
typedef struct _EActivityProxyClass EActivityProxyClass;
typedef struct _EActivityProxyPrivate EActivityProxyPrivate;

struct _EActivityProxy {
	GtkEventBox parent;
	EActivityProxyPrivate *priv;
};

struct _EActivityProxyClass {
	GtkEventBoxClass parent_class;
};

GType		e_activity_proxy_get_type	(void);
GtkWidget *	e_activity_proxy_new		(EActivity *activity);
EActivity *	e_activity_proxy_get_activity	(EActivityProxy *proxy);

G_END_DECLS

#endif /* E_ACTIVITY_PROXY_H */
