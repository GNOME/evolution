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

#include <glib-object.h>
#include <bonobo/Bonobo.h>

G_BEGIN_DECLS

#define E_COMPONENT_LISTENER_TYPE        (e_component_listener_get_type ())
#define E_COMPONENT_LISTENER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_COMPONENT_LISTENER_TYPE, EComponentListener))
#define E_COMPONENT_LISTENER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_COMPONENT_LISTENER_TYPE, EComponentListenerClass))
#define E_IS_COMPONENT_LISTENER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_COMPONENT_LISTENER_TYPE))
#define E_IS_COMPONENT_LISTENER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_COMPONENT_LISTENER_TYPE))

typedef struct _EComponentListenerPrivate EComponentListenerPrivate;

typedef struct {
	GObject object;
	EComponentListenerPrivate *priv;
} EComponentListener;

typedef struct {
	GObjectClass parent_class;

	void (* component_died) (EComponentListener *cl);
} EComponentListenerClass;

GType               e_component_listener_get_type       (void);
EComponentListener *e_component_listener_new            (Bonobo_Unknown comp);

Bonobo_Unknown      e_component_listener_get_component  (EComponentListener *cl);
void                e_component_listener_set_component  (EComponentListener *cl,
							 Bonobo_Unknown comp);

G_END_DECLS

#endif
