/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-view.c
 *
 * Copyright (C) 2000, 2001, 2002  Ximian, Inc.
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

#include <gtk/gtksignal.h>

#include <gal/util/e-util.h>

#include "evolution-shell-view.h"

#include "e-shell-marshal.h"


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _EvolutionShellViewPrivate {
	int dummy;
};

enum {
	SET_MESSAGE,
	UNSET_MESSAGE,
	CHANGE_VIEW,
	SET_TITLE,
	SET_FOLDER_BAR_LABEL,
	SHOW_SETTINGS,
	LAST_SIGNAL
};
static int signals[LAST_SIGNAL] = { 0 };


/* CORBA interface implementation.  */

static void
impl_ShellView_set_message (PortableServer_Servant servant,
			    const CORBA_char *message,
			    const CORBA_boolean busy,
			    CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;

	bonobo_object = bonobo_object_from_servant (servant);
	g_signal_emit (bonobo_object, signals[SET_MESSAGE], 0, message, busy);
}

static void
impl_ShellView_unset_message (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;

	bonobo_object = bonobo_object_from_servant (servant);
	g_signal_emit (bonobo_object, signals[UNSET_MESSAGE], 0);
}

static void
impl_ShellView_change_current_view (PortableServer_Servant servant,
				    const CORBA_char *uri,
				    CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;

	bonobo_object = bonobo_object_from_servant (servant);
	g_signal_emit (bonobo_object, signals[CHANGE_VIEW], 0, uri);
}

static void
impl_ShellView_set_title (PortableServer_Servant servant,
			  const CORBA_char *title,
			  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;

	bonobo_object = bonobo_object_from_servant (servant);
	g_signal_emit (bonobo_object, signals[SET_TITLE], 0, title);
}

static void
impl_ShellView_set_folder_bar_label (PortableServer_Servant servant,
				     const CORBA_char  *text,
				     CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;

	bonobo_object = bonobo_object_from_servant (servant);
	g_signal_emit (bonobo_object, signals[SET_FOLDER_BAR_LABEL], 0, text);
}

static void
impl_ShellView_show_settings (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;

	bonobo_object = bonobo_object_from_servant (servant);
	g_signal_emit (bonobo_object, signals[SHOW_SETTINGS], 0);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	/* Nothing to do here.  */

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EvolutionShellView *shell_view;
	EvolutionShellViewPrivate *priv;

	shell_view = EVOLUTION_SHELL_VIEW (object);
	priv = shell_view->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
evolution_shell_view_class_init (EvolutionShellViewClass *klass)
{
	POA_GNOME_Evolution_ShellView__epv *epv;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	epv = &klass->epv;
	epv->setMessage        = impl_ShellView_set_message;
	epv->unsetMessage      = impl_ShellView_unset_message;
	epv->changeCurrentView = impl_ShellView_change_current_view;
	epv->setTitle          = impl_ShellView_set_title;
	epv->setFolderBarLabel = impl_ShellView_set_folder_bar_label;
	epv->showSettings      = impl_ShellView_show_settings;

	signals[SET_MESSAGE]
		= g_signal_new ("set_message",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionShellViewClass, set_message),
				NULL, NULL,
				e_shell_marshal_NONE__STRING_BOOL,
				G_TYPE_NONE, 2,
				G_TYPE_STRING,
				G_TYPE_BOOLEAN);

	signals[UNSET_MESSAGE]
		= g_signal_new ("unset_message",
				  G_OBJECT_CLASS_TYPE (object_class),
				  G_SIGNAL_RUN_FIRST,
				  G_STRUCT_OFFSET (EvolutionShellViewClass, unset_message),
				  NULL, NULL,
				  e_shell_marshal_NONE__NONE,
				  G_TYPE_NONE, 0);

	signals[CHANGE_VIEW]
		= g_signal_new ("change_current_view",
				  G_OBJECT_CLASS_TYPE (object_class),
				  G_SIGNAL_RUN_FIRST,
				  G_STRUCT_OFFSET (EvolutionShellViewClass, change_current_view),
				  NULL, NULL,
				  e_shell_marshal_NONE__STRING,
				  G_TYPE_NONE, 1,
				  G_TYPE_STRING);

	signals[SET_TITLE]
		= g_signal_new ("set_title",
				  G_OBJECT_CLASS_TYPE (object_class),
				  G_SIGNAL_RUN_FIRST,
				  G_STRUCT_OFFSET (EvolutionShellViewClass, set_title),
				  NULL, NULL,
				  e_shell_marshal_NONE__STRING,
				  G_TYPE_NONE, 1,
				  G_TYPE_STRING);

	signals[SET_FOLDER_BAR_LABEL]
		= g_signal_new ("set_folder_bar_label",
				  G_OBJECT_CLASS_TYPE (object_class),
				  G_SIGNAL_RUN_FIRST,
				  G_STRUCT_OFFSET (EvolutionShellViewClass, set_folder_bar_label),
				  NULL, NULL,
				  e_shell_marshal_NONE__STRING,
				  G_TYPE_NONE, 1,
				  G_TYPE_STRING);

	signals[SHOW_SETTINGS]
		= g_signal_new ("show_settings",
				  G_OBJECT_CLASS_TYPE (object_class),
				  G_SIGNAL_RUN_FIRST,
				  G_STRUCT_OFFSET (EvolutionShellViewClass, show_settings),
				  NULL, NULL,
				  e_shell_marshal_NONE__NONE,
				  G_TYPE_NONE, 0);

	parent_class = g_type_class_ref(bonobo_object_get_type ());
}

static void
evolution_shell_view_init (EvolutionShellView *shell_view)
{
	EvolutionShellViewPrivate *priv;

	priv = g_new (EvolutionShellViewPrivate, 1);
	priv->dummy = 0;

	shell_view->priv = priv;
}



/**
 * evolution_shell_view_new:
 *
 * Create a new EvolutionShellView object.
 * 
 * Return value: The new EvolutionShellView object.
 **/
EvolutionShellView *
evolution_shell_view_new (void)
{
	return g_object_new (evolution_shell_view_get_type (), NULL);
}


BONOBO_TYPE_FUNC_FULL (EvolutionShellView,
		       GNOME_Evolution_ShellView,
		       PARENT_TYPE,
		       evolution_shell_view)
