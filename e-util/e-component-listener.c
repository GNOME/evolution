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
#define DEFAULT_PING_DELAY 10000

struct _EComponentListenerPrivate {
	Bonobo_Unknown component;
	int ping_delay;
	int ping_timeout_id;
};

static void e_component_listener_class_init (EComponentListenerClass *klass);
static void e_component_listener_init       (EComponentListener *cl, EComponentListenerClass *klass);
static void e_component_listener_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

enum {
	COMPONENT_DIED,
	LAST_SIGNAL
};

static guint comp_listener_signals[LAST_SIGNAL];

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
	cl->priv->ping_delay = DEFAULT_PING_DELAY;
	cl->priv->ping_timeout_id = -1;
}

static void
e_component_listener_finalize (GObject *object)
{
	EComponentListener *cl = (EComponentListener *) object;

	g_return_if_fail (E_IS_COMPONENT_LISTENER (cl));

	cl->priv->component = CORBA_OBJECT_NIL;

	if (cl->priv->ping_timeout_id != -1) {
		g_source_remove (cl->priv->ping_timeout_id);
		cl->priv->ping_timeout_id = -1;
	}

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

static gboolean
ping_component_callback (gpointer user_data)
{
	gboolean alive;
	int is_nil;
	CORBA_Environment ev;
	EComponentListener *cl = (EComponentListener *) user_data;

	g_return_val_if_fail (E_IS_COMPONENT_LISTENER (cl), FALSE);

	if (cl->priv->component == CORBA_OBJECT_NIL)
		return FALSE;

	CORBA_exception_init (&ev);
	is_nil = CORBA_Object_is_nil (cl->priv->component, &ev);
	if (BONOBO_EX (&ev)) {
		g_message (_("ping_timeout_callback: could not determine if the "
			     "CORBA object is nil or not"));
		goto out;
	}

	if (is_nil)
		goto out;

	alive = bonobo_unknown_ping (cl->priv->component, &ev);
	if (alive) {
		CORBA_exception_free (&ev);
		return TRUE;
	}

 out:
	/* the component has died, so we notify and close the timeout */
	CORBA_exception_free (&ev);

	/* we ref the object just in case it gets destroyed in the callbacks */
	g_object_ref (G_OBJECT (cl));
	g_signal_emit (G_OBJECT (cl), comp_listener_signals[COMPONENT_DIED], 0);

	cl->priv->component = CORBA_OBJECT_NIL;
	cl->priv->ping_timeout_id = -1;

	g_object_unref (G_OBJECT (cl));

	return FALSE;
}

static void
setup_ping_timeout (EComponentListener *cl)
{
	if (cl->priv->ping_timeout_id != -1)
		g_source_remove (cl->priv->ping_timeout_id);

	cl->priv->ping_timeout_id = g_timeout_add (cl->priv->ping_delay,
						   ping_component_callback,
						   cl);
}

/**
 * e_component_listener_new
 * @comp: Component to listen for.
 * @ping_delay: Delay (in ms) for pinging the component.
 *
 * Create a new #EComponentListener object, which allows to listen
 * for a given component and get notified when that component dies.
 *
 * Returns: a component listener object.
 */
EComponentListener *
e_component_listener_new (Bonobo_Unknown comp, int ping_delay)
{
	EComponentListener *cl;

	cl = g_object_new (E_COMPONENT_LISTENER_TYPE, NULL);
	cl->priv->component = comp;

	/* set up the timeout function */
	cl->priv->ping_delay = ping_delay > 0 ? ping_delay : DEFAULT_PING_DELAY;
	setup_ping_timeout (cl);

	return cl;
}

/**
 * e_component_listener_get_ping_delay
 * @cl: A #EComponentListener object.
 *
 * Get the ping delay being used to listen for an object.
 */
int
e_component_listener_get_ping_delay (EComponentListener *cl)
{
	g_return_val_if_fail (E_IS_COMPONENT_LISTENER (cl), -1);
	return cl->priv->ping_delay;
}

void
e_component_listener_set_ping_delay (EComponentListener *cl, int ping_delay)
{
	g_return_if_fail (E_IS_COMPONENT_LISTENER (cl));
	g_return_if_fail (ping_delay > 0);

	cl->priv->ping_delay = ping_delay;
	setup_ping_timeout (cl);
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
	setup_ping_timeout (cl);
}
