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

	EvolutionShellComponentCreateViewFn   create_view_fn;
	EvolutionShellComponentCreateFolderFn create_folder_fn;
	EvolutionShellComponentRemoveFolderFn remove_folder_fn;

	EvolutionShellClient *owner_client;

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
	Evolution_Shell shell_duplicate;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	if (priv->owner_client != NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_ShellComponent_AlreadyOwned, NULL);
		return;
	}

	shell_duplicate = CORBA_Object_duplicate (shell, ev);

	if (ev->_major == CORBA_NO_EXCEPTION) {
		priv->owner_client = evolution_shell_client_new (shell_duplicate);
		gtk_signal_emit (GTK_OBJECT (shell_component), signals[OWNER_SET], priv->owner_client);
	}
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

	if (priv->owner_client == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Evolution_ShellComponent_NotOwned, NULL);
		return;
	}

	bonobo_object_unref (BONOBO_OBJECT (priv->owner_client));
	priv->owner_client = NULL;

	gtk_signal_emit (GTK_OBJECT (shell_component), signals[OWNER_UNSET]);
}

static Bonobo_Control
impl_ShellComponent_create_view (PortableServer_Servant servant,
				 const CORBA_char *physical_uri,
				 const CORBA_char *type,
				 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	EvolutionShellComponentResult result;
	BonoboControl *control;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	result = (* priv->create_view_fn) (shell_component, physical_uri, type,
					   &control, priv->closure);

	if (result != EVOLUTION_SHELL_COMPONENT_OK) {
		switch (result) {
		case EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE:
			CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
					     ex_Evolution_ShellComponent_UnsupportedType,
					     NULL);
			break;
		case EVOLUTION_SHELL_COMPONENT_INTERNALERROR:
			CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
					     ex_Evolution_ShellComponent_InternalError,
					     NULL);
			break;
		default:
			CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
					     ex_Evolution_ShellComponent_NotFound,
					     NULL);
		}

		return CORBA_OBJECT_NIL;
	}

	return bonobo_object_corba_objref (BONOBO_OBJECT (control));
}

static void
impl_ShellComponent_async_create_folder (PortableServer_Servant servant,
					 const Evolution_ShellComponentListener listener,
					 const CORBA_char *physical_uri,
					 const CORBA_char *type,
					 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	if (priv->create_folder_fn == NULL) {
		Evolution_ShellComponentListener_report_result (listener,
								Evolution_ShellComponentListener_UNSUPPORTED_OPERATION,
								ev);
		return;
	}

	(* priv->create_folder_fn) (shell_component, physical_uri, type, listener, priv->closure);
}

static void
impl_ShellComponent_async_remove_folder (PortableServer_Servant servant,
					 const Evolution_ShellComponentListener listener,
					 const CORBA_char *physical_uri,
					 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	if (priv->remove_folder_fn == NULL) {
		Evolution_ShellComponentListener_report_result (listener,
								Evolution_ShellComponentListener_UNSUPPORTED_OPERATION,
								ev);
		return;
	}

	(* priv->remove_folder_fn) (shell_component, physical_uri, listener, priv->closure);
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

	if (priv->owner_client != NULL)
		bonobo_object_unref (BONOBO_OBJECT (priv->owner_client));

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
	epv->async_create_folder  = impl_ShellComponent_async_create_folder;
	epv->async_remove_folder  = impl_ShellComponent_async_remove_folder;

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

	priv->folder_types     = NULL;
	priv->create_view_fn   = NULL;
	priv->create_folder_fn = NULL;
	priv->remove_folder_fn = NULL;
	priv->owner_client     = NULL;
	priv->closure          = NULL;

	shell_component->priv = priv;
}


void
evolution_shell_component_construct (EvolutionShellComponent *shell_component,
				     const EvolutionShellComponentFolderType folder_types[],
				     Evolution_ShellComponent corba_object,
				     EvolutionShellComponentCreateViewFn create_view_fn,
				     EvolutionShellComponentCreateFolderFn create_folder_fn,
				     EvolutionShellComponentRemoveFolderFn remove_folder_fn,
				     void *closure)
{
	EvolutionShellComponentPrivate *priv;
	int i;

	g_return_if_fail (shell_component != NULL);
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);

	bonobo_object_construct (BONOBO_OBJECT (shell_component), corba_object);

	priv = shell_component->priv;

	priv->create_view_fn   = create_view_fn;
	priv->create_folder_fn = create_folder_fn;
	priv->remove_folder_fn = remove_folder_fn;

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
			       EvolutionShellComponentCreateFolderFn create_folder_fn,
			       EvolutionShellComponentRemoveFolderFn remove_folder_fn,
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
	evolution_shell_component_construct (new, folder_types, corba_object,
					     create_view_fn, create_folder_fn, remove_folder_fn,
					     closure);

	return new;
}

EvolutionShellClient *
evolution_shell_component_get_owner  (EvolutionShellComponent *shell_component)
{
	g_return_val_if_fail (shell_component != NULL, NULL);
	g_return_val_if_fail (EVOLUTION_IS_SHELL_COMPONENT (shell_component), NULL);

	return shell_component->priv->owner_client;
}


E_MAKE_TYPE (evolution_shell_component, "EvolutionShellComponent", EvolutionShellComponent,
	     class_init, init, PARENT_TYPE)
