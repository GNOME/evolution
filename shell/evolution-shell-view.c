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


#define PARENT_TYPE bonobo_x_object_get_type ()
static BonoboXObjectClass *parent_class = NULL;

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
	g_signal_emit (GTK_OBJECT (bonobo_object), signals[CHANGE_VIEW], 0, uri);
}

static void
impl_ShellView_set_title (PortableServer_Servant servant,
			  const CORBA_char *title,
			  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;

	bonobo_object = bonobo_object_from_servant (servant);
	g_signal_emit (GTK_OBJECT (bonobo_object), signals[SET_TITLE], 0, title);
}

static void
impl_ShellView_set_folder_bar_label (PortableServer_Servant servant,
				     const CORBA_char  *text,
				     CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;

	bonobo_object = bonobo_object_from_servant (servant);
	g_signal_emit (GTK_OBJECT (bonobo_object), signals[SET_FOLDER_BAR_LABEL], 0, text);
}

static void
impl_ShellView_show_settings (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;

	bonobo_object = bonobo_object_from_servant (servant);
	g_signal_emit (GTK_OBJECT (bonobo_object), signals[SHOW_SETTINGS], 0);
}


/* GObject methods.  */
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
class_init (EvolutionShellViewClass *klass)
{
	POA_GNOME_Evolution_ShellView__epv *epv;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = impl_finalize;

	epv = &klass->epv;
	epv->setMessage        = impl_ShellView_set_message;
	epv->unsetMessage      = impl_ShellView_unset_message;
	epv->changeCurrentView = impl_ShellView_change_current_view;
	epv->setTitle          = impl_ShellView_set_title;
	epv->setFolderBarLabel = impl_ShellView_set_folder_bar_label;
	epv->showSettings      = impl_ShellView_show_settings;

	signals[SET_MESSAGE]
		= gtk_signal_new ("set_message",
				  GTK_RUN_FIRST,
				  GTK_CLASS_TYPE (object_class),
				  GTK_SIGNAL_OFFSET (EvolutionShellViewClass, set_message),
				  gtk_marshal_NONE__POINTER_INT,
				  GTK_TYPE_NONE, 2,
				  GTK_TYPE_STRING,
				  GTK_TYPE_BOOL);

	signals[UNSET_MESSAGE]
		= gtk_signal_new ("unset_message",
				  GTK_RUN_FIRST,
				  GTK_CLASS_TYPE (object_class),
				  GTK_SIGNAL_OFFSET (EvolutionShellViewClass, unset_message),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	signals[CHANGE_VIEW]
		= gtk_signal_new ("change_current_view",
				  GTK_RUN_FIRST,
				  GTK_CLASS_TYPE (object_class),
				  GTK_SIGNAL_OFFSET (EvolutionShellViewClass, change_current_view),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_STRING);

	signals[SET_TITLE]
		= gtk_signal_new ("set_title",
				  GTK_RUN_FIRST,
				  GTK_CLASS_TYPE (object_class),
				  GTK_SIGNAL_OFFSET (EvolutionShellViewClass, set_title),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_STRING);

	signals[SET_FOLDER_BAR_LABEL]
		= gtk_signal_new ("set_folder_bar_label",
				  GTK_RUN_FIRST,
				  GTK_CLASS_TYPE (object_class),
				  GTK_SIGNAL_OFFSET (EvolutionShellViewClass, set_folder_bar_label),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_STRING);

	signals[SHOW_SETTINGS]
		= gtk_signal_new ("show_settings",
				  GTK_RUN_FIRST,
				  GTK_CLASS_TYPE (object_class),
				  GTK_SIGNAL_OFFSET (EvolutionShellViewClass, show_settings),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	parent_class = gtk_type_class (bonobo_x_object_get_type ());
}

static void
init (EvolutionShellView *shell_view)
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
	return gtk_type_new (evolution_shell_view_get_type ());
}


E_MAKE_X_TYPE (evolution_shell_view, "EvolutionShellView", EvolutionShellView,
	       class_init, init, PARENT_TYPE,
	       POA_GNOME_Evolution_ShellView__init,
	       GTK_STRUCT_OFFSET (EvolutionShellViewClass, epv))
