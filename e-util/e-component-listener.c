/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Component listener.
 *
 * Author:
 *   Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2002, Ximian, Inc.
 */

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-object.h>
#include "e-component-listener.h"
#include <libgnome/gnome-i18n.h>

#define PARENT_TYPE GTK_TYPE_OBJECT

struct _EComponentListenerPrivate {
	Bonobo_Unknown component;
};

static void e_component_listener_class_init (EComponentListenerClass *klass);
static void e_component_listener_init       (EComponentListener *cl, EComponentListenerClass *klass);
static void e_component_listener_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;
static GList *watched_connections = NULL;

enum {
	COMPONENT_DIED,
	LAST_SIGNAL
};

static guint comp_listener_signals[LAST_SIGNAL];

static void
connection_listen_cb (gpointer object, gpointer user_data)
{
	GList *l, *next = NULL;
	EComponentListener *cl;

	for (l = watched_connections; l != NULL; l = next) {
		next = l->next;
		cl = l->data;

		switch (ORBit_small_get_connection_status (cl->priv->component)) {
		case ORBIT_CONNECTION_DISCONNECTED :
			watched_connections = g_list_delete_link (watched_connections, l);

			g_object_ref (cl);
			g_signal_emit (cl, comp_listener_signals[COMPONENT_DIED], 0);
			cl->priv->component = CORBA_OBJECT_NIL;
			g_object_unref (cl);
			break;
		default :
		}
	}
}

static void
e_component_listener_class_init (EComponentListenerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = e_component_listener_finalize;
	klass->component_died = NULL;

	comp_listener_signals[COMPONENT_DIED] =
		g_signal_new ("component_died",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EComponentListenerClass, component_died),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
e_component_listener_init (EComponentListener *cl, EComponentListenerClass *klass)
{
	/* allocate internal structure */
	cl->priv = g_new (EComponentListenerPrivate, 1);
	cl->priv->component = CORBA_OBJECT_NIL;
}

static void
e_component_listener_finalize (GObject *object)
{
	EComponentListener *cl = (EComponentListener *) object;

	g_return_if_fail (E_IS_COMPONENT_LISTENER (cl));

	watched_connections = g_list_remove (watched_connections, cl);

	if (cl->priv->component != CORBA_OBJECT_NIL)
		cl->priv->component = CORBA_OBJECT_NIL;

	/* free memory */
	g_free (cl->priv);
	cl->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

GType
e_component_listener_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
                        sizeof (EComponentListenerClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) e_component_listener_class_init,
                        NULL, NULL,
                        sizeof (EComponentListener),
                        0,
                        (GInstanceInitFunc) e_component_listener_init
                };
		type = g_type_register_static (G_TYPE_OBJECT, "EComponentListener", &info, 0);
	}

	return type;
}

/**
 * e_component_listener_new
 * @comp: Component to listen for.
 *
 * Create a new #EComponentListener object, which allows to listen
 * for a given component and get notified when that component dies.
 *
 * Returns: a component listener object.
 */
EComponentListener *
e_component_listener_new (Bonobo_Unknown comp)
{
	EComponentListener *cl;

	g_return_val_if_fail (comp != NULL, NULL);

	cl = g_object_new (E_COMPONENT_LISTENER_TYPE, NULL);
	cl->priv->component = comp;

	/* watch the connection */
	ORBit_small_listen_for_broken (comp, G_CALLBACK (connection_listen_cb), cl);
	watched_connections = g_list_prepend (watched_connections, cl);

	return cl;
}

Bonobo_Unknown
e_component_listener_get_component (EComponentListener *cl)
{
	g_return_val_if_fail (E_IS_COMPONENT_LISTENER (cl), CORBA_OBJECT_NIL);
	return cl->priv->component;
}

void
e_component_listener_set_component (EComponentListener *cl, Bonobo_Unknown comp)
{
	g_return_if_fail (E_IS_COMPONENT_LISTENER (cl));

	cl->priv->component = comp;
	ORBit_small_listen_for_broken (comp, G_CALLBACK (connection_listen_cb), cl);
}
