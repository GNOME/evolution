/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-config-control.c
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-config-control.h"

#include <gal/util/e-util.h>

#include <gtk/gtksignal.h>

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-event-source.h>


#define PARENT_TYPE BONOBO_X_OBJECT_TYPE
static BonoboXObjectClass *parent_class = NULL;

struct _EvolutionConfigControlPrivate {
	gboolean changed;
	BonoboControl *control;
	BonoboEventSource *event_source;
};

enum {
	APPLY,
	LAST_SIGNAL
};
static int signals[LAST_SIGNAL] = { 0 };


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EvolutionConfigControl *config_control;
	EvolutionConfigControlPrivate *priv;

	config_control = EVOLUTION_CONFIG_CONTROL (object);
	priv = config_control->priv;

	if (priv->control != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->control));
		priv->control = NULL;
	}

	if (priv->event_source != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->event_source));
		priv->event_source = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EvolutionConfigControl *config_control;
	EvolutionConfigControlPrivate *priv;

	config_control = EVOLUTION_CONFIG_CONTROL (object);
	priv = config_control->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Evolution::ConfigControl CORBA methods.  */

static void
impl_apply (PortableServer_Servant servant,
	    CORBA_Environment *ev)
{
	EvolutionConfigControl *config_control;
	EvolutionConfigControlPrivate *priv;

	config_control = EVOLUTION_CONFIG_CONTROL (bonobo_object_from_servant (servant));
	priv = config_control->priv;

	g_signal_emit (GTK_OBJECT (config_control), signals[APPLY], 0);

	priv->changed = FALSE;
}

static Bonobo_Control
impl__get_control (PortableServer_Servant servant,
		   CORBA_Environment *ev)
{
	EvolutionConfigControl *config_control;
	EvolutionConfigControlPrivate *priv;

	config_control = EVOLUTION_CONFIG_CONTROL (bonobo_object_from_servant (servant));
	priv = config_control->priv;

	bonobo_object_ref (BONOBO_OBJECT (priv->control));

	return CORBA_Object_duplicate (bonobo_object_corba_objref (BONOBO_OBJECT (priv->control)), ev);
}

static Bonobo_EventSource
impl__get_eventSource (PortableServer_Servant servant,
		       CORBA_Environment *ev)
{
	EvolutionConfigControl *config_control;
	EvolutionConfigControlPrivate *priv;

	config_control = EVOLUTION_CONFIG_CONTROL (bonobo_object_from_servant (servant));
	priv = config_control->priv;

	bonobo_object_ref (BONOBO_OBJECT (priv->event_source));

	return CORBA_Object_duplicate (bonobo_object_corba_objref (BONOBO_OBJECT (priv->event_source)), ev);
}


static void
class_init (EvolutionConfigControlClass *class)
{
	POA_GNOME_Evolution_ConfigControl__epv *epv;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	epv = &class->epv;
	epv->apply            = impl_apply;
	epv->_get_control     = impl__get_control;
	epv->_get_eventSource = impl__get_eventSource;

	signals[APPLY] = gtk_signal_new ("apply", GTK_RUN_FIRST,
					 GTK_CLASS_TYPE (object_class),
					 GTK_SIGNAL_OFFSET (EvolutionConfigControlClass, apply),
					 gtk_marshal_NONE__NONE,
					 GTK_TYPE_NONE, 0);

	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
init (EvolutionConfigControl *config_control)
{
	EvolutionConfigControlPrivate *priv;

	priv = g_new (EvolutionConfigControlPrivate, 1);
	priv->changed      = FALSE;
	priv->control      = NULL;
	priv->event_source = bonobo_event_source_new ();

	config_control->priv = priv;
}


void
evolution_config_control_construct (EvolutionConfigControl *control,
				    GtkWidget *widget)
{
	EvolutionConfigControlPrivate *priv;

	g_return_if_fail (EVOLUTION_IS_CONFIG_CONTROL (control));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	priv = control->priv;

	priv->control = bonobo_control_new (widget);
}

EvolutionConfigControl *
evolution_config_control_new (GtkWidget *widget)
{
	EvolutionConfigControl *new;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	new = g_object_new (evolution_config_control_get_type (), NULL);
	evolution_config_control_construct (new, widget);

	return new;
}

void
evolution_config_control_changed (EvolutionConfigControl *config_control)
{
	EvolutionConfigControlPrivate *priv;
	CORBA_Environment ev;
	CORBA_any *null_value;

	g_return_if_fail (EVOLUTION_IS_CONFIG_CONTROL (config_control));

	priv = config_control->priv;

	if (priv->changed)
		return;

	priv->changed = TRUE;

	CORBA_exception_init (&ev);

	null_value = CORBA_any__alloc ();
	null_value->_type = TC_null;

	bonobo_event_source_notify_listeners (priv->event_source, "changed", null_value, &ev);

	CORBA_free (null_value);

	CORBA_exception_free (&ev);
}


E_MAKE_X_TYPE (evolution_config_control, "EvolutionConfigControl", EvolutionConfigControl,
	       class_init, init, PARENT_TYPE,
	       POA_GNOME_Evolution_ConfigControl__init,
	       GTK_STRUCT_OFFSET (EvolutionConfigControlClass, epv))
