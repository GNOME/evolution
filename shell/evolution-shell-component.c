/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <bonobo.h>

#include "Evolution.h"

#include "e-util/e-util.h"

#include "evolution-shell-component.h"


#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _EvolutionShellComponentPrivate {
	GList *folder_types;	/* EvolutionShellComponentFolderType */

	EvolutionShellComponentCreateViewFn create_view_fn;
	Evolution_Shell corba_owner;
	void *closure;
};

enum {
	OWNER_SET,
	OWNER_UNSET,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* CORBA interface implementation.  */

static POA_Evolution_ShellComponent__vepv ShellComponent_vepv;

static POA_Evolution_ShellComponent *
create_servant (void)
{
	POA_Evolution_ShellComponent *servant;
	CORBA_Environment ev;

	servant = (POA_Evolution_ShellComponent *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &ShellComponent_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_ShellComponent__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static Evolution_FolderTypeList *
impl_ShellComponent__get_supported_types (PortableServer_Servant servant,
					  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	Evolution_FolderTypeList *folder_type_list;
	unsigned int i;
	GList *p;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	folder_type_list = Evolution_FolderTypeList__alloc ();
	folder_type_list->_length = g_list_length (priv->folder_types);
	folder_type_list->_maximum = folder_type_list->_length;
	folder_type_list->_buffer = CORBA_sequence_Evolution_FolderType_allocbuf (folder_type_list->_maximum);

	for (p = priv->folder_types, i = 0; p != NULL; p = p->next, i++) {
		Evolution_FolderType *corba_folder_type;
		EvolutionShellComponentFolderType *folder_type;

		folder_type = (EvolutionShellComponentFolderType *) p->data;

		corba_folder_type = folder_type_list->_buffer + i;
		corba_folder_type->name      = CORBA_string_dup (folder_type->name);
		corba_folder_type->icon_name = CORBA_string_dup (folder_type->icon_name);
	}

	return folder_type_list;
}

static void
impl_ShellComponent_set_owner (PortableServer_Servant servant,
			       const Evolution_Shell shell,
			       CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	if (priv->corba_owner != CORBA_OBJECT_NIL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_ShellComponent_AlreadyOwned, NULL);
		return;
	}

	Bonobo_Unknown_ref (shell, ev);
	priv->corba_owner = CORBA_Object_duplicate (shell, ev);

	gtk_signal_emit (GTK_OBJECT (shell_component), signals[OWNER_SET], priv->corba_owner);
}

static void
impl_ShellComponent_unset_owner (PortableServer_Servant servant,
				 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	if (priv->corba_owner == CORBA_OBJECT_NIL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_ShellComponent_NotOwned, NULL);
		return;
	}

	Bonobo_Unknown_unref (priv->corba_owner, ev);
	CORBA_Object_release (priv->corba_owner, ev);

	gtk_signal_emit (GTK_OBJECT (shell_component), signals[OWNER_UNSET]);
}

static Bonobo_Control
impl_ShellComponent_create_view (PortableServer_Servant servant,
				 const CORBA_char *physical_uri,
				 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	BonoboControl *control;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	control = (* priv->create_view_fn) (shell_component, physical_uri, priv->closure);

	if (control == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_ShellComponent_NotFound,
				     NULL);
		return CORBA_OBJECT_NIL;
	}

	return bonobo_object_corba_objref (BONOBO_OBJECT (control));
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	CORBA_Environment ev;
	GList *p;

	shell_component = EVOLUTION_SHELL_COMPONENT (object);

	priv = shell_component->priv;

	CORBA_exception_init (&ev);

	if (priv->corba_owner != NULL) {
		Bonobo_Unknown_unref (priv->corba_owner, &ev);
		CORBA_Object_release (priv->corba_owner, &ev);
	}

	CORBA_exception_free (&ev);

	for (p = priv->folder_types; p != NULL; p = p->next) {
		EvolutionShellComponentFolderType *folder_type;

		folder_type = (EvolutionShellComponentFolderType *) p->data;

		g_free (folder_type->name);
		g_free (folder_type->icon_name);
		g_free (folder_type);
	}
	g_list_free (priv->folder_types);

	g_free (priv);
}


/* Initialization.  */

static void
corba_class_init (void)
{
	POA_Evolution_ShellComponent__vepv *vepv;
	POA_Evolution_ShellComponent__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_Evolution_ShellComponent__epv, 1);
	epv->_get_supported_types = impl_ShellComponent__get_supported_types;
	epv->set_owner            = impl_ShellComponent_set_owner;
	epv->unset_owner          = impl_ShellComponent_unset_owner;
	epv->create_view          = impl_ShellComponent_create_view;

	vepv = &ShellComponent_vepv;
	vepv->_base_epv                    = base_epv;
	vepv->Bonobo_Unknown_epv           = bonobo_object_get_epv ();
	vepv->Evolution_ShellComponent_epv = epv;
}

static void
class_init (EvolutionShellComponentClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	signals[OWNER_SET]
		= gtk_signal_new ("owner_set",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionShellComponentClass, owner_set),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);

	signals[OWNER_UNSET]
		= gtk_signal_new ("owner_unset",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionShellComponentClass, owner_unset),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	parent_class = gtk_type_class (PARENT_TYPE);

	corba_class_init ();
}

static void
init (EvolutionShellComponent *shell_component)
{
	EvolutionShellComponentPrivate *priv;

	priv = g_new (EvolutionShellComponentPrivate, 1);

	priv->folder_types    = NULL;
	priv->create_view_fn  = NULL;
	priv->closure         = NULL;
	priv->corba_owner     = CORBA_OBJECT_NIL;

	shell_component->priv = priv;
}


void
evolution_shell_component_construct (EvolutionShellComponent *shell_component,
				     const EvolutionShellComponentFolderType folder_types[],
				     Evolution_ShellComponent corba_object,
				     EvolutionShellComponentCreateViewFn create_view_fn,
				     void *closure)
{
	EvolutionShellComponentPrivate *priv;
	int i;

	g_return_if_fail (shell_component != NULL);
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);

	bonobo_object_construct (BONOBO_OBJECT (shell_component), corba_object);

	priv = shell_component->priv;

	priv->create_view_fn = create_view_fn;
	priv->closure = closure;

	for (i = 0; folder_types[i].name != NULL; i++) {
		EvolutionShellComponentFolderType *new;

		if (folder_types[i].icon_name == NULL
		    || folder_types[i].name[0] == '\0'
		    || folder_types[i].icon_name[0] == '\0')
			continue;

		new = g_new (EvolutionShellComponentFolderType, 1);
		new->name      = g_strdup (folder_types[i].name);
		new->icon_name = g_strdup (folder_types[i].icon_name);

		priv->folder_types = g_list_prepend (priv->folder_types, new);
	}

	if (priv->folder_types == NULL)
		g_warning ("No valid folder types constructing EShellComponent %p", shell_component);
}

EvolutionShellComponent *
evolution_shell_component_new (const EvolutionShellComponentFolderType folder_types[],
			       EvolutionShellComponentCreateViewFn create_view_fn,
			       void *closure)
{
	EvolutionShellComponent *new;
	POA_Evolution_ShellComponent *servant;
	Evolution_ShellComponent corba_object;

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	new = gtk_type_new (evolution_shell_component_get_type ());

	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (new), servant);
	evolution_shell_component_construct (new, folder_types, corba_object, create_view_fn, closure);

	return new;
}

Evolution_Shell
evolution_shell_component_get_owner  (EvolutionShellComponent *shell_component)
{
	g_return_val_if_fail (shell_component != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EVOLUTION_IS_SHELL_COMPONENT (shell_component), CORBA_OBJECT_NIL);

	return shell_component->priv->corba_owner;
}


E_MAKE_TYPE (evolution_shell_component, "EvolutionShellComponent", EvolutionShellComponent,
	     class_init, init, PARENT_TYPE)
