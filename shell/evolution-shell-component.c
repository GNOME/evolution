/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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

#include <fcntl.h>

#include <gtk/gtksignal.h>
#include <bonobo/bonobo-object.h>

#include <gal/util/e-util.h>

#include "Evolution.h"

#include "evolution-shell-component.h"


#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

static GtkObjectClass *parent_class = NULL;

struct _EvolutionShellComponentPrivate {
	GList *folder_types;	/* EvolutionShellComponentFolderType */

	EvolutionShellComponentCreateViewFn create_view_fn;
	EvolutionShellComponentCreateFolderFn create_folder_fn;
	EvolutionShellComponentRemoveFolderFn remove_folder_fn;
	EvolutionShellComponentXferFolderFn xfer_folder_fn;
	EvolutionShellComponentPopulateFolderContextMenuFn populate_folder_context_menu_fn;
	EvolutionShellComponentGetDndSelectionFn get_dnd_selection_fn;

	EvolutionShellClient *owner_client;

	void *closure;
};

enum {
	OWNER_SET,
	OWNER_UNSET,
	DEBUG,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* Helper functions.  */

/* Notice that, if passed a NULL pointer, this string will construct a
   zero-element NULL-terminated string array instead of returning NULL itself
   (i.e. it will return a pointer to a single g_malloc()ed NULL pointer).  */
static char **
duplicate_null_terminated_string_array (char *array[])
{
	char **new;
	int count;
	int i;

	if (array == NULL) {
		count = 0;
	} else {
		for (count = 0; array[count] != NULL; count++)
			;
	}

	new = g_new (char *, count + 1);

	for (i = 0; i < count; i++)
		new[i] = g_strdup (array[i]);
	new[count] = NULL;

	return new;
}

/* The following will create a CORBA sequence of strings from the specified
 * NULL-terminated array, without duplicating the strings.  */
static void
fill_corba_sequence_from_null_terminated_string_array (CORBA_sequence_CORBA_string *corba_sequence,
						       char **array)
{
	int count;
	int i;

	g_assert (corba_sequence != NULL);
	g_assert (array != NULL);

	/* We won't be reallocating the strings, so we don't want them to be
	   freed when the sequence is freed.  */
	CORBA_sequence_set_release (corba_sequence, FALSE);

	count = 0;
	while (array[count] != NULL)
		count++;

	corba_sequence->_maximum = count;
	corba_sequence->_length = count;
	corba_sequence->_buffer = CORBA_sequence_CORBA_string_allocbuf (count);

	for (i = 0; i < count; i++)
		corba_sequence->_buffer[i] = (CORBA_char *) array[i];
}


/* CORBA interface implementation.  */

static GNOME_Evolution_FolderTypeList *
impl_ShellComponent__get_supported_types (PortableServer_Servant servant,
					  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	GNOME_Evolution_FolderTypeList *folder_type_list;
	unsigned int i;
	GList *p;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	folder_type_list = GNOME_Evolution_FolderTypeList__alloc ();
	CORBA_sequence_set_release (folder_type_list, TRUE);
	folder_type_list->_length = g_list_length (priv->folder_types);
	folder_type_list->_maximum = folder_type_list->_length;
	folder_type_list->_buffer = CORBA_sequence_GNOME_Evolution_FolderType_allocbuf (folder_type_list->_maximum);

	for (p = priv->folder_types, i = 0; p != NULL; p = p->next, i++) {
		GNOME_Evolution_FolderType *corba_folder_type;
		EvolutionShellComponentFolderType *folder_type;

		folder_type = (EvolutionShellComponentFolderType *) p->data;

		corba_folder_type = folder_type_list->_buffer + i;
		corba_folder_type->name      = CORBA_string_dup (folder_type->name);
		corba_folder_type->icon_name = CORBA_string_dup (folder_type->icon_name);

		fill_corba_sequence_from_null_terminated_string_array (& corba_folder_type->accepted_dnd_types,
								       folder_type->accepted_dnd_types);
		fill_corba_sequence_from_null_terminated_string_array (& corba_folder_type->exported_dnd_types,
								       folder_type->exported_dnd_types);
	}

	return folder_type_list;
}

static void
impl_ShellComponent_set_owner (PortableServer_Servant servant,
			       const GNOME_Evolution_Shell shell,
			       const CORBA_char *evolution_homedir,
			       CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	GNOME_Evolution_Shell shell_duplicate;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	if (priv->owner_client != NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_ShellComponent_AlreadyOwned, NULL);
		return;
	}

	shell_duplicate = CORBA_Object_duplicate (shell, ev);

	if (ev->_major == CORBA_NO_EXCEPTION) {
		priv->owner_client = evolution_shell_client_new (shell_duplicate);
		gtk_signal_emit (GTK_OBJECT (shell_component), signals[OWNER_SET], priv->owner_client, evolution_homedir);
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
				     ex_GNOME_Evolution_ShellComponent_NotOwned, NULL);
		return;
	}

	bonobo_object_unref (BONOBO_OBJECT (priv->owner_client));
	priv->owner_client = NULL;

	gtk_signal_emit (GTK_OBJECT (shell_component), signals[OWNER_UNSET]);
}

static void
impl_ShellComponent_debug (PortableServer_Servant servant,
			   const CORBA_char *log_path,
			   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	int fd;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);

	fd = open (log_path, O_WRONLY | O_APPEND);
	if (!fd)
		return;

	dup2 (fd, STDOUT_FILENO);
	dup2 (fd, STDERR_FILENO);
	close (fd);

	gtk_signal_emit (GTK_OBJECT (shell_component), signals[DEBUG]);
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
					     ex_GNOME_Evolution_ShellComponent_UnsupportedType,
					     NULL);
			break;
		case EVOLUTION_SHELL_COMPONENT_INTERNALERROR:
			CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
					     ex_GNOME_Evolution_ShellComponent_InternalError,
					     NULL);
			break;
		default:
			CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
					     ex_GNOME_Evolution_ShellComponent_NotFound,
					     NULL);
		}

		return CORBA_OBJECT_NIL;
	}

	return CORBA_Object_duplicate (bonobo_object_corba_objref (BONOBO_OBJECT (control)), ev);
}

static void
impl_ShellComponent_async_create_folder (PortableServer_Servant servant,
					 const GNOME_Evolution_ShellComponentListener listener,
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
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_OPERATION,
								     ev);
		return;
	}

	(* priv->create_folder_fn) (shell_component, physical_uri, type, listener, priv->closure);
}

static void
impl_ShellComponent_async_remove_folder (PortableServer_Servant servant,
					 const GNOME_Evolution_ShellComponentListener listener,
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
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_OPERATION,
								     ev);
		return;
	}

	(* priv->remove_folder_fn) (shell_component, physical_uri, listener, priv->closure);
}

static void
impl_ShellComponent_async_xfer_folder (PortableServer_Servant servant,
				       const GNOME_Evolution_ShellComponentListener listener,
				       const CORBA_char *source_physical_uri,
				       const CORBA_char *destination_physical_uri,
				       const CORBA_boolean remove_source,
				       CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	if (priv->xfer_folder_fn == NULL) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_OPERATION,
								     ev);
		return;
	}

	(* priv->xfer_folder_fn) (shell_component,
				  source_physical_uri,
				  destination_physical_uri,
				  remove_source,
				  listener,
				  priv->closure);
}

static void
impl_ShellComponent_populate_folder_context_menu (PortableServer_Servant servant,
						  const Bonobo_UIContainer corba_uih,
						  const CORBA_char *physical_uri,
						  const CORBA_char *type,
						  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	BonoboUIComponent *uic;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	if (priv->populate_folder_context_menu_fn == NULL)
		return;

	uic = bonobo_ui_component_new_default ();
	bonobo_ui_component_set_container (uic, corba_uih);
	bonobo_object_release_unref (corba_uih, NULL);

	(* priv->populate_folder_context_menu_fn) (shell_component, uic, physical_uri, type, priv->closure);

	bonobo_object_unref (BONOBO_OBJECT (uic));
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
		g_strfreev (folder_type->exported_dnd_types);
		g_strfreev (folder_type->accepted_dnd_types);

		g_free (folder_type);
	}
	g_list_free (priv->folder_types);

	g_free (priv);

	parent_class->destroy (object);
}


/* Initialization.  */

static void
class_init (EvolutionShellComponentClass *klass)
{
	GtkObjectClass *object_class;
	POA_GNOME_Evolution_ShellComponent__epv *epv = &klass->epv;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	signals[OWNER_SET]
		= gtk_signal_new ("owner_set",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionShellComponentClass, owner_set),
				  gtk_marshal_NONE__POINTER_POINTER,
				  GTK_TYPE_NONE, 2,
				  GTK_TYPE_POINTER, GTK_TYPE_POINTER);

	signals[OWNER_UNSET]
		= gtk_signal_new ("owner_unset",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionShellComponentClass, owner_unset),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	signals[DEBUG]
		= gtk_signal_new ("debug",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionShellComponentClass, debug),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	parent_class = gtk_type_class (PARENT_TYPE);

	epv->_get_supported_types      = impl_ShellComponent__get_supported_types;
	epv->setOwner                  = impl_ShellComponent_set_owner;
	epv->unsetOwner                = impl_ShellComponent_unset_owner;
	epv->debug                     = impl_ShellComponent_debug;
	epv->createView                = impl_ShellComponent_create_view;
	epv->createFolderAsync         = impl_ShellComponent_async_create_folder;
	epv->removeFolderAsync         = impl_ShellComponent_async_remove_folder;
	epv->xferFolderAsync           = impl_ShellComponent_async_xfer_folder;
	epv->populateFolderContextMenu = impl_ShellComponent_populate_folder_context_menu;
}

static void
init (EvolutionShellComponent *shell_component)
{
	EvolutionShellComponentPrivate *priv;

	priv = g_new (EvolutionShellComponentPrivate, 1);

	priv->folder_types                    = NULL;
	priv->create_view_fn                  = NULL;
	priv->create_folder_fn                = NULL;
	priv->remove_folder_fn                = NULL;
	priv->xfer_folder_fn                  = NULL;
	priv->populate_folder_context_menu_fn = NULL;

	priv->owner_client                    = NULL;
	priv->closure                         = NULL;

	shell_component->priv = priv;
}


void
evolution_shell_component_construct (EvolutionShellComponent *shell_component,
				     const EvolutionShellComponentFolderType folder_types[],
				     EvolutionShellComponentCreateViewFn create_view_fn,
				     EvolutionShellComponentCreateFolderFn create_folder_fn,
				     EvolutionShellComponentRemoveFolderFn remove_folder_fn,
				     EvolutionShellComponentXferFolderFn xfer_folder_fn,
				     EvolutionShellComponentPopulateFolderContextMenuFn populate_folder_context_menu_fn,
				     EvolutionShellComponentGetDndSelectionFn get_dnd_selection_fn,
				     void *closure)
{
	EvolutionShellComponentPrivate *priv;
	int i;

	g_return_if_fail (shell_component != NULL);

	priv = shell_component->priv;

	priv->create_view_fn                  = create_view_fn;
	priv->create_folder_fn                = create_folder_fn;
	priv->remove_folder_fn                = remove_folder_fn;
	priv->xfer_folder_fn                  = xfer_folder_fn;
	priv->populate_folder_context_menu_fn = populate_folder_context_menu_fn;
	priv->get_dnd_selection_fn            = get_dnd_selection_fn;

	priv->closure = closure;

	for (i = 0; folder_types[i].name != NULL; i++) {
		EvolutionShellComponentFolderType *new;

		if (folder_types[i].icon_name == NULL
		    || folder_types[i].name[0] == '\0'
		    || folder_types[i].icon_name[0] == '\0')
			continue;

		new = g_new (EvolutionShellComponentFolderType, 1);
		new->name               = g_strdup (folder_types[i].name);
		new->icon_name          = g_strdup (folder_types[i].icon_name);
		new->accepted_dnd_types = duplicate_null_terminated_string_array (folder_types[i].accepted_dnd_types);
		new->exported_dnd_types = duplicate_null_terminated_string_array (folder_types[i].exported_dnd_types);

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
			       EvolutionShellComponentXferFolderFn xfer_folder_fn,
			       EvolutionShellComponentPopulateFolderContextMenuFn populate_folder_context_menu_fn,
			       EvolutionShellComponentGetDndSelectionFn get_dnd_selection_fn,
			       void *closure)
{
	EvolutionShellComponent *new;

	new = gtk_type_new (evolution_shell_component_get_type ());

	evolution_shell_component_construct (new,
					     folder_types,
					     create_view_fn,
					     create_folder_fn,
					     remove_folder_fn,
					     xfer_folder_fn,
					     populate_folder_context_menu_fn,
					     get_dnd_selection_fn,
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


E_MAKE_X_TYPE (evolution_shell_component, "EvolutionShellComponent", EvolutionShellComponent,
	       class_init, init, PARENT_TYPE,
	       POA_GNOME_Evolution_ShellComponent__init,
	       GTK_STRUCT_OFFSET (EvolutionShellComponentClass, epv));
