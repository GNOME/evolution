/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * evolution-wizard.c
 *
 * Copyright (C) 2000, 2001, 2002 Ximian, Inc.
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
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtktypeutils.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-event-source.h>

#include "evolution-wizard.h"
#include "Evolution.h"

#include "e-shell-marshal.h"

struct _EvolutionWizardPrivate {
	EvolutionWizardGetControlFn get_fn;
	BonoboEventSource *event_source;

	void *closure;
	int page_count;
};

enum {
	NEXT,
	PREPARE,
	BACK,
	FINISH,
	CANCEL,
	HELP,
	LAST_SIGNAL
};

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

static GtkObjectClass *parent_class;
static guint32 signals[LAST_SIGNAL] = { 0 };

static CORBA_long
impl_GNOME_Evolution_Wizard__get_pageCount (PortableServer_Servant servant,
					    CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionWizard *wizard;
	EvolutionWizardPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	wizard = EVOLUTION_WIZARD (bonobo_object);
	priv = wizard->priv;

	return priv->page_count;
}

static Bonobo_Control
impl_GNOME_Evolution_Wizard_getControl (PortableServer_Servant servant,
					CORBA_long pagenumber,
					CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionWizard *wizard;
	EvolutionWizardPrivate *priv;
	BonoboControl *control;

	bonobo_object = bonobo_object_from_servant (servant);
	wizard = EVOLUTION_WIZARD (bonobo_object);
	priv = wizard->priv;

	if (pagenumber < 0 || pagenumber >= priv->page_count) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Wizard_NoPage, NULL);
		return CORBA_OBJECT_NIL;
	}

	control = priv->get_fn (wizard, pagenumber, priv->closure);
	if (control == NULL)
		return CORBA_OBJECT_NIL;

	return (Bonobo_Control) CORBA_Object_duplicate (BONOBO_OBJREF (control), ev);
}

static void
impl_GNOME_Evolution_Wizard_notifyAction (PortableServer_Servant servant,
					  CORBA_long pagenumber,
					  GNOME_Evolution_Wizard_Action action,
					  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionWizard *wizard;
	EvolutionWizardPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	wizard = EVOLUTION_WIZARD (bonobo_object);
	priv = wizard->priv;

	if (pagenumber < 0
	    || pagenumber > priv->page_count
	    || (action != GNOME_Evolution_Wizard_BACK && pagenumber == priv->page_count)) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Wizard_NoPage, NULL);
		return;
	}

	switch (action) {
	case GNOME_Evolution_Wizard_NEXT:
		g_signal_emit (bonobo_object, signals[NEXT], 0, pagenumber);
		break;

	case GNOME_Evolution_Wizard_PREPARE:
		g_signal_emit (bonobo_object, signals[PREPARE], 0, pagenumber);
		break;

	case GNOME_Evolution_Wizard_BACK:
		g_signal_emit (bonobo_object, signals[BACK], 0, pagenumber);
		break;

	case GNOME_Evolution_Wizard_FINISH:
		g_signal_emit (bonobo_object, signals[FINISH], 0, pagenumber);
		break;

	case GNOME_Evolution_Wizard_CANCEL:
		g_signal_emit (bonobo_object, signals[CANCEL], 0, pagenumber);
		break;

	case GNOME_Evolution_Wizard_HELP:
		g_signal_emit (bonobo_object, signals[HELP], 0, pagenumber);
		break;

	default:
		break;
	}
}


static void
impl_dispose (GObject *object)
{
	/* (Nothing to do here.)  */

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EvolutionWizard *wizard;

	wizard = EVOLUTION_WIZARD (object);
	if (wizard->priv == NULL)
		return;

	g_free (wizard->priv);
	wizard->priv = NULL;

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
evolution_wizard_class_init (EvolutionWizardClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_Wizard__epv *epv = &klass->epv;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	signals[NEXT] 
		= g_signal_new ("next",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionWizardClass, next),
				NULL, NULL,
				e_shell_marshal_NONE__INT,
				G_TYPE_NONE, 1,
				G_TYPE_INT);
	signals[PREPARE] 
		= g_signal_new ("prepare",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionWizardClass, prepare),
				NULL, NULL,
				e_shell_marshal_NONE__INT,
				G_TYPE_NONE, 1,
				G_TYPE_INT);
	signals[BACK] 
		= g_signal_new ("back", 
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionWizardClass, back),
				NULL, NULL,
				e_shell_marshal_NONE__INT,
				G_TYPE_NONE, 1,
				G_TYPE_INT);
	signals[FINISH] 
		= g_signal_new ("finish", 
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionWizardClass, finish),
				NULL, NULL,
				e_shell_marshal_NONE__INT,
				G_TYPE_NONE, 1,
				G_TYPE_INT);
	signals[CANCEL] 
		= g_signal_new ("cancel", 
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionWizardClass, cancel),
				NULL, NULL,
				e_shell_marshal_NONE__INT,
				G_TYPE_NONE, 1,
				G_TYPE_INT);
	signals[HELP] 
		= g_signal_new ("help", 
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionWizardClass, help),
				NULL, NULL,
				e_shell_marshal_NONE__INT,
				G_TYPE_NONE, 1,
				G_TYPE_INT);

	parent_class = g_type_class_ref(PARENT_TYPE);

	epv->_get_pageCount = impl_GNOME_Evolution_Wizard__get_pageCount;
	epv->getControl = impl_GNOME_Evolution_Wizard_getControl;
	epv->notifyAction = impl_GNOME_Evolution_Wizard_notifyAction;
}

static void
evolution_wizard_init (EvolutionWizard *wizard)
{
	wizard->priv = g_new0 (EvolutionWizardPrivate, 1);
}

BONOBO_X_TYPE_FUNC_FULL (EvolutionWizard, GNOME_Evolution_Wizard, 
			 PARENT_TYPE, evolution_wizard);

EvolutionWizard *
evolution_wizard_construct (EvolutionWizard *wizard,
			    BonoboEventSource *event_source,
			    EvolutionWizardGetControlFn get_fn,
			    int num_pages,
			    void *closure)
{
	EvolutionWizardPrivate *priv;

	g_return_val_if_fail (BONOBO_IS_EVENT_SOURCE (event_source), NULL);
	g_return_val_if_fail (IS_EVOLUTION_WIZARD (wizard), NULL);

	priv = wizard->priv;
	priv->get_fn = get_fn;
	priv->page_count = num_pages;
	priv->closure = closure;

	priv->event_source = event_source;
	bonobo_object_add_interface (BONOBO_OBJECT (wizard),
				     BONOBO_OBJECT (event_source));

	return wizard;
}

EvolutionWizard *
evolution_wizard_new_full (EvolutionWizardGetControlFn get_fn,
			   int num_pages,
			   BonoboEventSource *event_source,
			   void *closure)
{
	EvolutionWizard *wizard;

	g_return_val_if_fail (num_pages > 0, NULL);
	g_return_val_if_fail (BONOBO_IS_EVENT_SOURCE (event_source), NULL);

	wizard = g_object_new (evolution_wizard_get_type (), NULL);

	return evolution_wizard_construct (wizard, event_source, get_fn, num_pages, closure);
}

EvolutionWizard *
evolution_wizard_new (EvolutionWizardGetControlFn get_fn,
		      int num_pages,
		      void *closure)
{
	BonoboEventSource *event_source;

	g_return_val_if_fail (num_pages > 0, NULL);

	event_source = bonobo_event_source_new ();

	return evolution_wizard_new_full (get_fn, num_pages, event_source, closure);
}

void
evolution_wizard_set_buttons_sensitive (EvolutionWizard *wizard,
					gboolean back_sensitive,
					gboolean next_sensitive,
					gboolean cancel_sensitive,
					CORBA_Environment *opt_ev)
{
	EvolutionWizardPrivate *priv;
	CORBA_Environment ev;
	CORBA_any any;
	CORBA_short s;

	g_return_if_fail (IS_EVOLUTION_WIZARD (wizard));

	priv = wizard->priv;

	if (opt_ev == NULL) {
		CORBA_exception_init (&ev);
	} else {
		ev = *opt_ev;
	}

	s = back_sensitive << 2 | next_sensitive << 1 | cancel_sensitive;
	any._type = (CORBA_TypeCode) TC_CORBA_short;
	any._value = &s;

	bonobo_event_source_notify_listeners (priv->event_source,
					      EVOLUTION_WIZARD_SET_BUTTONS_SENSITIVE,
					      &any, &ev);
	if (opt_ev == NULL && BONOBO_EX (&ev)) {
		g_warning ("ERROR(evolution_wizard_set_buttons_sensitive): %s", CORBA_exception_id (&ev));
	}

	if (opt_ev == NULL) {
		CORBA_exception_free (&ev);
	}
}

void
evolution_wizard_set_show_finish (EvolutionWizard *wizard,
				  gboolean show_finish,
				  CORBA_Environment *opt_ev)
{
	EvolutionWizardPrivate *priv;
	CORBA_Environment ev;
	CORBA_any any;
	CORBA_boolean b;

	g_return_if_fail (IS_EVOLUTION_WIZARD (wizard));

	priv = wizard->priv;
	if (opt_ev == NULL) {
		CORBA_exception_init (&ev);
	} else {
		ev = *opt_ev;
	}

	b = show_finish;
	any._type = (CORBA_TypeCode) TC_CORBA_boolean;
	any._value = &b;

	bonobo_event_source_notify_listeners (priv->event_source,
					      EVOLUTION_WIZARD_SET_SHOW_FINISH,
					      &any, &ev);
	if (opt_ev == NULL && BONOBO_EX (&ev)) {
		g_warning ("ERROR(evolution_wizard_set_show_finish): %s", CORBA_exception_id (&ev));
	}

	if (opt_ev == NULL) {
		CORBA_exception_free (&ev);
	}
}

void
evolution_wizard_set_page (EvolutionWizard *wizard,
			   int page_number,
			   CORBA_Environment *opt_ev)
{
	EvolutionWizardPrivate *priv;
	CORBA_Environment ev;
	CORBA_any any;
	CORBA_short s;

	g_return_if_fail (IS_EVOLUTION_WIZARD (wizard));

	priv = wizard->priv;

	g_return_if_fail (page_number >= 0 && page_number < priv->page_count);

	if (opt_ev == NULL) {
		CORBA_exception_init (&ev);
	} else {
		ev = *opt_ev;
	}

	s = page_number;
	any._type = (CORBA_TypeCode) TC_CORBA_short;
	any._value = &s;

	bonobo_event_source_notify_listeners (priv->event_source,
					      EVOLUTION_WIZARD_SET_PAGE,
					      &any, &ev);

	if (opt_ev == NULL && BONOBO_EX (&ev)) {
		g_warning ("ERROR(evolution_wizard_set_page): %s", CORBA_exception_id (&ev));
	}

	if (opt_ev == NULL) {
		CORBA_exception_free (&ev);
	}
}

BonoboEventSource *
evolution_wizard_get_event_source (EvolutionWizard *wizard)
{
	g_return_val_if_fail (IS_EVOLUTION_WIZARD (wizard), NULL);

	return wizard->priv->event_source;
}
