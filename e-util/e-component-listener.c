/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Component listener.
 *
 * Author:
 *   Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2002, Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtksignal.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-object.h>
#include <gal/util/e-util.h>
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
static void e_component_listener_init       (EComponentListener *cl);
static void e_component_listener_destroy    (GtkObject *object);

static GtkObjectClass *parent_class = NULL;

enum {
	COMPONENT_DIED,
	LAST_SIGNAL
};

static guint comp_listener_signals[LAST_SIGNAL];

static void
e_component_listener_class_init (EComponentListenerClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = e_component_listener_destroy;
	klass->component_died = NULL;

	comp_listener_signals[COMPONENT_DIED] =
		gtk_signal_new ("component_died",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EComponentListenerClass, component_died),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals (object_class, comp_listener_signals, LAST_SIGNAL);
}

static void
e_component_listener_init (EComponentListener *cl)
{
	/* allocate internal structure */
	cl->priv = g_new (EComponentListenerPrivate, 1);
	cl->priv->component = CORBA_OBJECT_NIL;
	cl->priv->ping_delay = DEFAULT_PING_DELAY;
	cl->priv->ping_timeout_id = -1;
}

static void
e_component_listener_destroy (GtkObject *object)
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

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

GtkType
e_component_listener_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info = {
			"EComponentListener",
			sizeof (EComponentListener),
			sizeof (EComponentListenerClass),
			(GtkClassInitFunc) e_component_listener_class_init,
			(GtkObjectInitFunc) e_component_listener_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
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
	CORBA_exception_free (&ev);

	if (is_nil)
		goto out;

	alive = bonobo_unknown_ping (cl->priv->component);
	if (alive)
		return TRUE;

 out:
	/* the component has died, so we notify and close the timeout */

	/* we ref the object just in case it gets destroyed in the callbacks */
	gtk_object_ref (GTK_OBJECT (cl));
	gtk_signal_emit (GTK_OBJECT (cl), comp_listener_signals[COMPONENT_DIED]);

	cl->priv->component = CORBA_OBJECT_NIL;
	cl->priv->ping_timeout_id = -1;

	gtk_object_unref (GTK_OBJECT (cl));

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

	cl = gtk_type_new (E_COMPONENT_LISTENER_TYPE);
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
