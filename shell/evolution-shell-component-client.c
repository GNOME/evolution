/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component-client.c
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

#include <gtk/gtksignal.h>
#include <gtk/gtktypeutils.h>

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-widget.h>

#include "e-util/e-util.h"

#include "evolution-shell-component-client.h"


#define PARENT_TYPE BONOBO_OBJECT_CLIENT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _EvolutionShellComponentClientPrivate {
	EvolutionShellComponentClientCallback callback;
	void *callback_data;

	Evolution_ShellComponentListener listener_interface;
	PortableServer_Servant listener_servant;
};


#define RETURN_ERROR_IF_FAIL(cond) \
	g_return_val_if_fail ((cond), EVOLUTION_SHELL_COMPONENT_INVALIDARG)


/* Object activation.  */

#ifdef USING_OAF

#include <liboaf/liboaf.h>

static CORBA_Object
activate_object_from_id (const char *id)
{
	CORBA_Environment ev;
	CORBA_Object corba_object;

	CORBA_exception_init (&ev);

	corba_object = oaf_activate_from_id ((char *) id, 0, NULL, &ev); /* Yuck.  */
	if (ev._major != CORBA_NO_EXCEPTION)
		corba_object = CORBA_OBJECT_NIL;

	CORBA_exception_free (&ev);

	return corba_object;
}

#else

#include <libgnorba/gnorba.h>

static CORBA_Object
activate_object_from_id (const char *id)
{
	return goad_server_activate_with_id (NULL, id, 0, NULL);
}

#endif


/* Utility functions.  */

static EvolutionShellComponentResult
corba_exception_to_result (const CORBA_Environment *ev)
{
	if (ev->_major == CORBA_NO_EXCEPTION)
		return EVOLUTION_SHELL_COMPONENT_OK;

	if (ev->_major == CORBA_USER_EXCEPTION) {
		if (strcmp (ev->_repo_id, ex_Evolution_ShellComponent_AlreadyOwned) == 0)
			return EVOLUTION_SHELL_COMPONENT_ALREADYOWNED;
		if (strcmp (ev->_repo_id, ex_Evolution_ShellComponent_NotOwned) == 0)
			return EVOLUTION_SHELL_COMPONENT_NOTOWNED;
		if (strcmp (ev->_repo_id, ex_Evolution_ShellComponent_NotFound) == 0)
			return EVOLUTION_SHELL_COMPONENT_NOTFOUND;
		if (strcmp (ev->_repo_id, ex_Evolution_ShellComponent_UnsupportedType) == 0)
			return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;
		if (strcmp (ev->_repo_id, ex_Evolution_ShellComponent_InternalError) == 0)
			return EVOLUTION_SHELL_COMPONENT_INTERNALERROR;
		if (strcmp (ev->_repo_id, ex_Evolution_ShellComponent_Busy) == 0)
			return EVOLUTION_SHELL_COMPONENT_BUSY;

		return EVOLUTION_SHELL_COMPONENT_UNKNOWNERROR;
	} else {
		/* FIXME maybe we need something more specific here.  */
		return EVOLUTION_SHELL_COMPONENT_CORBAERROR;
	}
}

static void
dispatch_callback (EvolutionShellComponentClient *shell_component_client,
		   EvolutionShellComponentResult result)
{
	EvolutionShellComponentClientPrivate *priv;
	EvolutionShellComponentClientCallback callback;
	PortableServer_ObjectId *oid;
	void *callback_data;
	CORBA_Environment ev;

	priv = shell_component_client->priv;

	g_return_if_fail (priv->callback != NULL);
	g_return_if_fail (priv->listener_servant != NULL);

	/* Notice that we destroy the interface and reset the callback information before
           dispatching the callback so that the callback can generate another request.  */

	CORBA_exception_init (&ev);

	oid = PortableServer_POA_servant_to_id (bonobo_poa (), priv->listener_servant, &ev);
	PortableServer_POA_deactivate_object (bonobo_poa (), oid, &ev);
	POA_Evolution_ShellComponentListener__fini (priv->listener_servant, &ev);
	CORBA_free (oid);

	CORBA_Object_release (priv->listener_interface, &ev);

	CORBA_exception_free (&ev);

	priv->listener_servant   = NULL;
	priv->listener_interface = CORBA_OBJECT_NIL;

	callback      = priv->callback;
	callback_data = priv->callback_data;

	priv->callback      = NULL;
	priv->callback_data = NULL;

	(* callback) (shell_component_client, result, callback_data);
}


/* CORBA listener interface implementation.  */

static PortableServer_ServantBase__epv            ShellComponentListener_base_epv;
static POA_Evolution_ShellComponentListener__epv  ShellComponentListener_epv;
static POA_Evolution_ShellComponentListener__vepv ShellComponentListener_vepv;
static gboolean ShellComponentListener_vepv_initialized = FALSE;

struct _ShellComponentListenerServant {
	POA_Evolution_ShellComponentListener servant;
	EvolutionShellComponentClient *component_client;
};
typedef struct _ShellComponentListenerServant ShellComponentListenerServant;

static EvolutionShellComponentClient *
component_client_from_ShellComponentListener_servant (PortableServer_Servant servant)
{
	ShellComponentListenerServant *listener_servant;

	listener_servant = (ShellComponentListenerServant *) servant;
	return listener_servant->component_client;
}

static EvolutionShellComponentResult
result_from_async_corba_result (Evolution_ShellComponentListener_Result async_corba_result)
{
	switch (async_corba_result) {
	case Evolution_ShellComponentListener_OK:
		return EVOLUTION_SHELL_COMPONENT_OK;
	case Evolution_ShellComponentListener_UNSUPPORTED_OPERATION:
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDOPERATION;
	case Evolution_ShellComponentListener_EXISTS:
		return EVOLUTION_SHELL_COMPONENT_EXISTS;
	case Evolution_ShellComponentListener_INVALID_URI:
		return EVOLUTION_SHELL_COMPONENT_INVALIDURI;
	case Evolution_ShellComponentListener_PERMISSION_DENIED:
		return EVOLUTION_SHELL_COMPONENT_PERMISSIONDENIED;
	case Evolution_ShellComponentListener_HAS_SUBFOLDERS:
		return EVOLUTION_SHELL_COMPONENT_HASSUBFOLDERS;
	case Evolution_ShellComponentListener_NO_SPACE:
		return EVOLUTION_SHELL_COMPONENT_NOSPACE;
	default:
		return EVOLUTION_SHELL_COMPONENT_UNKNOWNERROR;
	}
}

static void
impl_ShellComponentListener_report_result (PortableServer_Servant servant,
					   const Evolution_ShellComponentListener_Result result,
					   CORBA_Environment *ev)
{
	EvolutionShellComponentClient *component_client;

	component_client = component_client_from_ShellComponentListener_servant (servant);
	dispatch_callback (component_client, result_from_async_corba_result (result));
}

static void
ShellComponentListener_vepv_initialize (void)
{
	ShellComponentListener_base_epv._private = NULL;
	ShellComponentListener_base_epv.finalize = NULL;
	ShellComponentListener_base_epv.default_POA = NULL;

	ShellComponentListener_epv.report_result = impl_ShellComponentListener_report_result;

	ShellComponentListener_vepv._base_epv = & ShellComponentListener_base_epv;
	ShellComponentListener_vepv.Evolution_ShellComponentListener_epv = & ShellComponentListener_epv;

	ShellComponentListener_vepv_initialized = TRUE;
}

static PortableServer_Servant *
create_ShellComponentListener_servant (EvolutionShellComponentClient *component_client)
{
	ShellComponentListenerServant *servant;

	if (! ShellComponentListener_vepv_initialized)
		ShellComponentListener_vepv_initialize ();

	servant = g_new0 (ShellComponentListenerServant, 1);
	servant->servant.vepv     = &ShellComponentListener_vepv;
	servant->component_client = component_client;

	return (PortableServer_Servant) servant;
}

static void
free_ShellComponentListener_servant (PortableServer_Servant servant)
{
	g_free (servant);
}

static void
create_listener_interface (EvolutionShellComponentClient *shell_component_client)
{
	EvolutionShellComponentClientPrivate *priv;
	PortableServer_Servant listener_servant;
	Evolution_ShellComponentListener corba_interface;
	CORBA_Environment ev;

	priv = shell_component_client->priv;

	listener_servant = create_ShellComponentListener_servant (shell_component_client);

	CORBA_exception_init (&ev);

	POA_Evolution_ShellComponentListener__init (listener_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		free_ShellComponentListener_servant (listener_servant);
		return;
	}

	CORBA_free (PortableServer_POA_activate_object (bonobo_poa (), listener_servant, &ev));

	corba_interface = PortableServer_POA_servant_to_reference (bonobo_poa (), listener_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		corba_interface = CORBA_OBJECT_NIL;
		free_ShellComponentListener_servant (listener_servant);
	}

	CORBA_exception_free (&ev);

	priv->listener_servant   = listener_servant;
	priv->listener_interface = corba_interface;
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EvolutionShellComponentClient *shell_component_client;
	EvolutionShellComponentClientPrivate *priv;

	shell_component_client = EVOLUTION_SHELL_COMPONENT_CLIENT (object);
	priv = shell_component_client->priv;

	if (priv->callback != NULL)
		dispatch_callback (shell_component_client, EVOLUTION_SHELL_COMPONENT_INTERRUPTED);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EvolutionShellComponentClientClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = impl_destroy;
}

static void
init (EvolutionShellComponentClient *shell_component_client)
{
	EvolutionShellComponentClientPrivate *priv;

	priv = g_new (EvolutionShellComponentClientPrivate, 1);
	priv->listener_interface = CORBA_OBJECT_NIL;
	priv->listener_servant   = NULL;
	priv->callback           = NULL;
	priv->callback_data      = NULL;

	shell_component_client->priv = priv;
}


/* Construction.  */

void
evolution_shell_component_client_construct (EvolutionShellComponentClient *shell_component_client,
					    CORBA_Object corba_object)
{
	g_return_if_fail (shell_component_client != NULL);
	g_return_if_fail (EVOLUTION_IS_SHELL_COMPONENT_CLIENT (shell_component_client));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);

	bonobo_object_client_construct (BONOBO_OBJECT_CLIENT (shell_component_client),
					corba_object);
}

EvolutionShellComponentClient *
evolution_shell_component_client_new (const char *id)
{
	EvolutionShellComponentClient *new;
	CORBA_Object corba_object;

	g_return_val_if_fail (id != NULL, NULL);

	new = gtk_type_new (evolution_shell_component_client_get_type ());

	corba_object = activate_object_from_id (id);

	if (corba_object == CORBA_OBJECT_NIL) {
		printf ("Could not activate component %s.\n"
			"(Maybe you need to set OAF_INFO_PATH?)\n"
			"CRASHING!\n", id);
		/* FIXME: This is not the right call here. It will SEGV
		 * in Bonobo_Unknown_unref.
		 */
		bonobo_object_unref (BONOBO_OBJECT (new));
		return NULL;
	}

	evolution_shell_component_client_construct (new, corba_object);

	return new;
}


/* Synchronous operations.  */

EvolutionShellComponentResult
evolution_shell_component_client_set_owner (EvolutionShellComponentClient *shell_component_client,
					    Evolution_Shell shell)
{
	EvolutionShellComponentResult result;
	CORBA_Environment ev;

	RETURN_ERROR_IF_FAIL (shell_component_client != NULL);
	RETURN_ERROR_IF_FAIL (EVOLUTION_IS_SHELL_COMPONENT_CLIENT (shell_component_client));
	RETURN_ERROR_IF_FAIL (shell != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	Evolution_ShellComponent_set_owner (bonobo_object_corba_objref (BONOBO_OBJECT (shell_component_client)),
					    shell, &ev);

	result = corba_exception_to_result (&ev);

	CORBA_exception_free (&ev);

	return result;
}

EvolutionShellComponentResult
evolution_shell_component_client_unset_owner (EvolutionShellComponentClient *shell_component_client,
					      Evolution_Shell shell)
{
	EvolutionShellComponentResult result;
	Evolution_ShellComponent corba_component;
	CORBA_Environment ev;

	RETURN_ERROR_IF_FAIL (shell_component_client != NULL);
	RETURN_ERROR_IF_FAIL (EVOLUTION_IS_SHELL_COMPONENT_CLIENT (shell_component_client));
	RETURN_ERROR_IF_FAIL (shell != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	corba_component = bonobo_object_corba_objref (BONOBO_OBJECT (shell_component_client));

	Evolution_ShellComponent_unset_owner (corba_component, &ev);

	result = corba_exception_to_result (&ev);

	CORBA_exception_free (&ev);

	return result;
}

EvolutionShellComponentResult
evolution_shell_component_client_create_view (EvolutionShellComponentClient *shell_component_client,
					      BonoboUIHandler *uih,
					      const char *physical_uri,
					      const char *type_string,
					      BonoboControl **control_return)
{
	EvolutionShellComponentResult result;
	CORBA_Environment ev;
	Evolution_ShellComponent corba_component;
	Bonobo_Control corba_control;

	RETURN_ERROR_IF_FAIL (shell_component_client != NULL);
	RETURN_ERROR_IF_FAIL (EVOLUTION_IS_SHELL_COMPONENT_CLIENT (shell_component_client));
	RETURN_ERROR_IF_FAIL (uih != NULL);
	RETURN_ERROR_IF_FAIL (BONOBO_IS_UI_HANDLER (uih));
	RETURN_ERROR_IF_FAIL (physical_uri != NULL);
	RETURN_ERROR_IF_FAIL (type_string != NULL);
	RETURN_ERROR_IF_FAIL (control_return != NULL);

	CORBA_exception_init (&ev);

	corba_component = bonobo_object_corba_objref (BONOBO_OBJECT (shell_component_client));
	corba_control = Evolution_ShellComponent_create_view (corba_component, physical_uri, type_string, &ev);

	result = corba_exception_to_result (&ev);

	if (result != EVOLUTION_SHELL_COMPONENT_OK) {
		*control_return = NULL;
	} else {
		Bonobo_UIHandler corba_uih;

		corba_uih = bonobo_object_corba_objref (BONOBO_OBJECT (uih));
		*control_return = BONOBO_CONTROL (bonobo_widget_new_control_from_objref (corba_control,
											 corba_uih));
	}

	CORBA_exception_free (&ev);

	return result;
}


/* Asyncronous operations.  */

void
evolution_shell_component_client_async_create_folder (EvolutionShellComponentClient *shell_component_client,
						      const char *physical_uri,
						      const char *type,
						      EvolutionShellComponentClientCallback callback,
						      void *data)
{
	EvolutionShellComponentClientPrivate *priv;
	Evolution_ShellComponent corba_shell_component;
	CORBA_Environment ev;

	priv = shell_component_client->priv;

	if (priv->callback != NULL) {
		(* callback) (shell_component_client, EVOLUTION_SHELL_COMPONENT_BUSY, data);
		return;
	}

	create_listener_interface (shell_component_client);

	CORBA_exception_init (&ev);

	corba_shell_component = bonobo_object_corba_objref (BONOBO_OBJECT (shell_component_client));

	priv->callback      = callback;
	priv->callback_data = data;

	Evolution_ShellComponent_async_create_folder (corba_shell_component,
						      priv->listener_interface,
						      physical_uri, type,
						      &ev);

	CORBA_exception_free (&ev);
}

void
evolution_shell_component_client_async_remove_folder (EvolutionShellComponentClient *shell_component_client,
						      const char *physical_uri,
						      EvolutionShellComponentClientCallback callback,
						      void *data)
{
}


E_MAKE_TYPE (evolution_shell_component_client, "EvolutionShellComponentClient",
	     EvolutionShellComponentClient, class_init, init, PARENT_TYPE)
