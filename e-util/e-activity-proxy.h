/*
 * e-activity-proxy.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

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
	GtkFrame parent;
	EActivityProxyPrivate *priv;
};

struct _EActivityProxyClass {
	GtkFrameClass parent_class;
};

GType		e_activity_proxy_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_activity_proxy_new		(EActivity *activity);
EActivity *	e_activity_proxy_get_activity	(EActivityProxy *proxy);
void		e_activity_proxy_set_activity	(EActivityProxy *proxy,
						 EActivity *activity);

G_END_DECLS

#endif /* E_ACTIVITY_PROXY_H */
