/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Component listener
 *
 * Author:
 *   Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2002, Ximian, Inc.
 */

#ifndef __E_COMPONENT_LISTENER_H__
#define __E_COMPONENT_LISTENER_H__

#include <glib/gmacros.h>
#include <bonobo/Bonobo.h>
#include <gtk/gtkobject.h>

G_BEGIN_DECLS

#define E_COMPONENT_LISTENER_TYPE        (e_component_listener_get_type ())
#define E_COMPONENT_LISTENER(o)          (GTK_CHECK_CAST ((o), E_COMPONENT_LISTENER_TYPE, EComponentListener))
#define E_COMPONENT_LISTENER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_COMPONENT_LISTENER_TYPE, EComponentListenerClass))
#define E_IS_COMPONENT_LISTENER(o)       (GTK_CHECK_TYPE ((o), E_COMPONENT_LISTENER_TYPE))
#define E_IS_COMPONENT_LISTENER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_COMPONENT_LISTENER_TYPE))

typedef struct _EComponentListenerPrivate EComponentListenerPrivate;

typedef struct {
	GtkObject object;
	EComponentListenerPrivate *priv;
} EComponentListener;

typedef struct {
	GtkObjectClass parent_class;

	void (* component_died) (EComponentListener *cl);
} EComponentListenerClass;

GtkType             e_component_listener_get_type       (void);
EComponentListener *e_component_listener_new            (Bonobo_Unknown comp, int ping_delay);

int                 e_component_listener_get_ping_delay (EComponentListener *cl);
void                e_component_listener_set_ping_delay (EComponentListener *cl, int ping_delay);
Bonobo_Unknown      e_component_listener_get_component  (EComponentListener *cl);
void                e_component_listener_set_component  (EComponentListener *cl,
							 Bonobo_Unknown comp);

G_END_DECLS

#endif
