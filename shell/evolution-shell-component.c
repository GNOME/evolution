/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component.c
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-shell-component.h"

#include "e-shell-corba-icon-utils.h"
#include "e-shell-marshal.h"

#include <fcntl.h>
#include <unistd.h>

#include <glib.h>
#include <gtk/gtksignal.h>
#include <bonobo/bonobo-object.h>
#include <libgnome/gnome-i18n.h>

#include <gal/util/e-util.h>


#define PING_DELAY 10000


#define PARENT_TYPE BONOBO_OBJECT_TYPE

static BonoboObjectClass *parent_class = NULL;

struct _UserCreatableItemType {
	char *id;
	char *description;
	char *menu_description;
	char *tooltip;
	char menu_shortcut;
	GdkPixbuf *icon;
	char *folder_type;
};
typedef struct _UserCreatableItemType UserCreatableItemType;

struct _EvolutionShellComponentPrivate {
	GList *folder_types;	/* EvolutionShellComponentFolderType */
	GList *external_uri_schemas; /* char * */

	EvolutionShellComponentCreateViewFn create_view_fn;
	EvolutionShellComponentCreateFolderFn create_folder_fn;
	EvolutionShellComponentRemoveFolderFn remove_folder_fn;
	EvolutionShellComponentXferFolderFn xfer_folder_fn;
	EvolutionShellComponentPopulateFolderContextMenuFn populate_folder_context_menu_fn;
	EvolutionShellComponentUnpopulateFolderContextMenuFn unpopulate_folder_context_menu_fn;
	EvolutionShellComponentGetDndSelectionFn get_dnd_selection_fn;
	EvolutionShellComponentRequestQuitFn request_quit_fn;

	EvolutionShellClient *owner_client;

	GSList *user_creatable_item_types; /* UserCreatableItemType */

	/* This is used for
	   populateFolderContextMenu/unpopulateFolderContextMenu.  */
	BonoboUIComponent *uic;

	gulong parent_view_xid;

	int ping_timeout_id;

	void *closure;
};

enum {
	OWNER_SET,
	OWNER_UNSET,
	OWNER_DIED,
	DEBUG,
	INTERACTIVE,
	HANDLE_EXTERNAL_URI,
	USER_CREATE_NEW_ITEM,
	SEND_RECEIVE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* UserCreatableItemType handling.  */

static UserCreatableItemType *
user_creatable_item_type_new (const char *id,
			      const char *description,
			      const char *menu_description,
			      const char *tooltip,
			      const char *folder_type,
			      char menu_shortcut,
			      GdkPixbuf *icon)
{
	UserCreatableItemType *type;

	type = g_new (UserCreatableItemType, 1);
	type->id               = g_strdup (id);
	type->description      = g_strdup (description);
	type->menu_description = g_strdup (menu_description);
	type->tooltip          = g_strdup (tooltip);
	type->menu_shortcut    = menu_shortcut;
	type->folder_type      = g_strdup (folder_type);

	if (icon == NULL)
		type->icon = NULL;
	else
		type->icon = g_object_ref (icon);

	return type;
}

static void
user_creatable_item_type_free (UserCreatableItemType *type)
{
	g_free (type->id);
	g_free (type->description);
	g_free (type->menu_description);
	g_free (type->folder_type);

	if (type->icon != NULL)
		g_object_unref (type->icon);

	g_free (type);
}


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

	CORBA_sequence_set_release (corba_sequence, TRUE);

	count = 0;
	while (array[count] != NULL)
		count++;

	corba_sequence->_maximum = count;
	corba_sequence->_length = count;
	corba_sequence->_buffer = CORBA_sequence_CORBA_string_allocbuf (count);

	for (i = 0; i < count; i++)
		corba_sequence->_buffer[i] = CORBA_string_dup (array[i]);

	CORBA_sequence_set_release (corba_sequence, TRUE);
}


/* Owner pinging.  */

static gboolean
owner_ping_callback (void *data)
{
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	Bonobo_Unknown owner_objref;
	CORBA_Environment ev;
	gboolean alive;

	shell_component = EVOLUTION_SHELL_COMPONENT (data);
	priv = shell_component->priv;

	owner_objref = evolution_shell_client_corba_objref (priv->owner_client);

	if (owner_objref == CORBA_OBJECT_NIL)
		return FALSE;

	/* We are duplicating the object here, as we might get an ::unsetOwner
	   while we invoke the pinging, and this would make the objref invalid
	   and thus crash the stubs (cfr. #13802).  */

	CORBA_exception_init (&ev);
	owner_objref = CORBA_Object_duplicate (owner_objref, &ev);

	alive = bonobo_unknown_ping (owner_objref, NULL);

	CORBA_Object_release (owner_objref, &ev);
	CORBA_exception_free (&ev);

	if (alive)
		return TRUE;

	/* This is tricky.  During the pinging, we might have gotten an
	   ::unsetOwner invocation which has invalidated our owner_client.  In
	   this case, no "owner_died" should be emitted.  */
	
	if (priv->owner_client != NULL) {
		g_print ("\t*** The shell has disappeared\n");
		g_signal_emit (shell_component, signals[OWNER_DIED], 0);
	}

	priv->ping_timeout_id = -1;

	return FALSE;
}

static void
setup_owner_pinging (EvolutionShellComponent *shell_component)
{
	EvolutionShellComponentPrivate *priv;
	GNOME_Evolution_Shell shell_corba_objref;

	priv = shell_component->priv;

	shell_corba_objref = evolution_shell_client_corba_objref (priv->owner_client);

	if (priv->ping_timeout_id != -1)
		g_source_remove (priv->ping_timeout_id);

	priv->ping_timeout_id = g_timeout_add (PING_DELAY, owner_ping_callback, shell_component);
}


/* CORBA interface implementation.  */

static GNOME_Evolution_FolderTypeList *
impl__get_supportedTypes (PortableServer_Servant servant,
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
		corba_folder_type->name          = CORBA_string_dup (folder_type->name);
		corba_folder_type->iconName      = CORBA_string_dup (folder_type->icon_name);
		corba_folder_type->displayName   = CORBA_string_dup (folder_type->display_name);
		corba_folder_type->description   = CORBA_string_dup (folder_type->description);
		corba_folder_type->userCreatable = folder_type->user_creatable;

		fill_corba_sequence_from_null_terminated_string_array (& corba_folder_type->acceptedDndTypes,
								       folder_type->accepted_dnd_types);
		fill_corba_sequence_from_null_terminated_string_array (& corba_folder_type->exportedDndTypes,
								       folder_type->exported_dnd_types);
	}

	CORBA_sequence_set_release (folder_type_list, TRUE);

	return folder_type_list;
}

static GNOME_Evolution_URISchemaList *
impl__get_externalUriSchemas (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	GNOME_Evolution_URISchemaList *uri_schema_list;
	GList *p;
	int i;

	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object_from_servant (servant));
	priv = shell_component->priv;

	uri_schema_list = GNOME_Evolution_URISchemaList__alloc ();

	/* FIXME: We could probably keep this to FALSE and avoid
	   CORBA_string_duplicating.  */
	CORBA_sequence_set_release (uri_schema_list, TRUE);

	if (priv->external_uri_schemas == NULL) {
		uri_schema_list->_length = 0;
		uri_schema_list->_maximum = 0;
		uri_schema_list->_buffer = NULL;
		return uri_schema_list;
	}

	uri_schema_list->_length = g_list_length (priv->external_uri_schemas);
	uri_schema_list->_maximum = uri_schema_list->_length;
	uri_schema_list->_buffer = CORBA_sequence_GNOME_Evolution_URISchema_allocbuf (uri_schema_list->_maximum);

	for (p = priv->external_uri_schemas, i = 0; p != NULL; p = p->next, i++) {
		const char *schema;

		schema = (const char *) p->data;
		uri_schema_list->_buffer[i] = CORBA_string_dup (schema);
	}

	CORBA_sequence_set_release (uri_schema_list, TRUE);

	return uri_schema_list;
}

static GNOME_Evolution_UserCreatableItemTypeList *
impl__get_userCreatableItemTypes (PortableServer_Servant servant,
				  CORBA_Environment *ev)
{
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	GNOME_Evolution_UserCreatableItemTypeList *list;
	GSList *p;
	int i;

	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object_from_servant (servant));
	priv = shell_component->priv;

	list = GNOME_Evolution_UserCreatableItemTypeList__alloc ();
	list->_maximum = g_slist_length (priv->user_creatable_item_types);
	list->_length  = list->_maximum;
	list->_buffer  = CORBA_sequence_GNOME_Evolution_UserCreatableItemType_allocbuf (list->_maximum);

	for (p = priv->user_creatable_item_types, i = 0; p != NULL; p = p->next, i ++) {
		GNOME_Evolution_UserCreatableItemType *corba_type;
		const UserCreatableItemType *type;

		corba_type = list->_buffer + i;
		type = (const UserCreatableItemType *) p->data;

		corba_type->id              = CORBA_string_dup (type->id);
		corba_type->description     = CORBA_string_dup (type->description);
		corba_type->menuDescription = CORBA_string_dup (type->menu_description);
		corba_type->tooltip         = CORBA_string_dup (type->tooltip != NULL ? type->tooltip : "");
		corba_type->folderType      = CORBA_string_dup (type->folder_type != NULL ? type->folder_type : "");
		corba_type->menuShortcut    = type->menu_shortcut;

		e_store_corba_icon_from_pixbuf (type->icon, & corba_type->icon);
	}

	CORBA_sequence_set_release (list, TRUE);

	return list;
}

static void
impl_setOwner (PortableServer_Servant servant,
	       const GNOME_Evolution_Shell shell,
	       const CORBA_char *evolution_homedir,
	       CORBA_Environment *ev)
{
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;

	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object_from_servant (servant));
	priv = shell_component->priv;

	if (priv->owner_client != NULL) {
		int owner_is_dead;

		owner_is_dead = CORBA_Object_non_existent
			(evolution_shell_client_corba_objref (priv->owner_client), ev);
		if (ev->_major != CORBA_NO_EXCEPTION)
			owner_is_dead = TRUE;

		if (! owner_is_dead) {
			CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
					     ex_GNOME_Evolution_ShellComponent_AlreadyOwned, NULL);
		} else {
			CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
					     ex_GNOME_Evolution_ShellComponent_OldOwnerHasDied, NULL);

			g_signal_emit (shell_component, signals[OWNER_DIED], 0);
		}

		return;
	}

	if (ev->_major == CORBA_NO_EXCEPTION) {
		BonoboObject *local_object;

		priv->owner_client = evolution_shell_client_new (shell);
		g_signal_emit (shell_component, signals[OWNER_SET], 0, priv->owner_client, evolution_homedir);

		/* Set up pinging of the shell (to realize if it's gone unexpectedly) when in the
		   non-local case.  */
		local_object = bonobo_object (ORBit_small_get_servant (shell));
		if (local_object == NULL)
			setup_owner_pinging (shell_component);
	}
}

static void
impl_unsetOwner (PortableServer_Servant servant,
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

	g_signal_emit (shell_component, signals[OWNER_UNSET], 0);
}

static void
impl_debug (PortableServer_Servant servant,
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

	g_signal_emit (shell_component, signals[DEBUG], 0);
}

static void
impl_interactive (PortableServer_Servant servant,
		  CORBA_boolean interactive,
		  CORBA_unsigned_long new_view_xid,
		  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);

	if (interactive)
		shell_component->priv->parent_view_xid = new_view_xid;
	else
		shell_component->priv->parent_view_xid = 0L;

	g_signal_emit (shell_component, signals[INTERACTIVE], 0,
		       interactive, new_view_xid);
}

static Bonobo_Control
impl_createView (PortableServer_Servant servant,
		 const CORBA_char *physical_uri,
		 const CORBA_char *type,
		 const CORBA_char *view_info,
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
					   view_info, &control, priv->closure);

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
impl_handleExternalURI (PortableServer_Servant servant,
			const CORBA_char *uri,
			CORBA_Environment *ev)
{
	EvolutionShellComponent *shell_component;

	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object_from_servant (servant));

	g_signal_emit (shell_component, signals[HANDLE_EXTERNAL_URI], 0, uri);
}

static void
impl_createFolderAsync (PortableServer_Servant servant,
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
impl_removeFolderAsync (PortableServer_Servant servant,
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

	if (priv->remove_folder_fn == NULL) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_OPERATION,
								     ev);
		return;
	}

	(* priv->remove_folder_fn) (shell_component, physical_uri, type, listener, priv->closure);
}

static void
impl_xferFolderAsync (PortableServer_Servant servant,
		      const GNOME_Evolution_ShellComponentListener listener,
		      const CORBA_char *source_physical_uri,
		      const CORBA_char *destination_physical_uri,
		      const CORBA_char *type,
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
				  type,
				  remove_source,
				  listener,
				  priv->closure);
}

static void
impl_populateFolderContextMenu (PortableServer_Servant servant,
				const Bonobo_UIContainer corba_uih,
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

	if (priv->populate_folder_context_menu_fn == NULL)
		return;

	if (priv->uic != NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_ShellComponent_AlreadyPopulated,
				     NULL);
		return;
	}

	priv->uic = bonobo_ui_component_new_default ();
	bonobo_ui_component_set_container (priv->uic, corba_uih, NULL);

	(* priv->populate_folder_context_menu_fn) (shell_component, priv->uic, physical_uri, type, priv->closure);
}

static void
impl_unpopulateFolderContextMenu (PortableServer_Servant servant,
				  const Bonobo_UIContainer corba_uih,
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

	if (priv->unpopulate_folder_context_menu_fn == NULL)
		return;

	if (priv->uic == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_ShellComponent_NotPopulated,
				     NULL);
		return;
	}

	(* priv->unpopulate_folder_context_menu_fn) (shell_component, priv->uic, physical_uri, type, priv->closure);

	bonobo_object_unref (BONOBO_OBJECT (priv->uic));
	priv->uic = NULL;
}

static void
impl_userCreateNewItem (PortableServer_Servant servant,
			const CORBA_char *id,
			const CORBA_char *parent_physical_uri,
			const CORBA_char *parent_type,
			CORBA_Environment *ev)
{
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;

	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object_from_servant (servant));
	priv = shell_component->priv;

	/* FIXME: Check that the type is good.  */

	g_signal_emit (shell_component, signals[USER_CREATE_NEW_ITEM], 0, id, parent_physical_uri, parent_type);
}

static void
impl_sendReceive (PortableServer_Servant servant,
		  const CORBA_boolean show_dialog,
		  CORBA_Environment *ev)
{
	EvolutionShellComponent *shell_component;

	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object_from_servant (servant));
	g_signal_emit (shell_component, signals[SEND_RECEIVE], 0, show_dialog);
}

static void
impl_requestQuit (PortableServer_Servant servant,
		  const GNOME_Evolution_ShellComponentListener listener,
		  CORBA_Environment *ev)
{
	EvolutionShellComponent *shell_component;
	gboolean allow_quit;

	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object_from_servant (servant));

	if (shell_component->priv->request_quit_fn == NULL)
		allow_quit = TRUE;
	else
		allow_quit = (* shell_component->priv->request_quit_fn) (shell_component,
									 shell_component->priv->closure);

	if (allow_quit)
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_OK,
								     ev);
	else
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_CANCEL,
								     ev);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;

	shell_component = EVOLUTION_SHELL_COMPONENT (object);

	priv = shell_component->priv;

	if (priv->ping_timeout_id != -1) {
		g_source_remove (priv->ping_timeout_id);
		priv->ping_timeout_id = -1;
	}

	if (priv->owner_client != NULL) {
		g_object_unref (priv->owner_client);
		priv->owner_client = NULL;
	}

	if (priv->uic != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->uic));
		priv->uic = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	GSList *sp;
	GList *p;

	shell_component = EVOLUTION_SHELL_COMPONENT (object);

	priv = shell_component->priv;

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

	e_free_string_list (priv->external_uri_schemas);

	for (sp = priv->user_creatable_item_types; sp != NULL; sp = sp->next)
		user_creatable_item_type_free ((UserCreatableItemType *) sp->data);
	g_slist_free (priv->user_creatable_item_types);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* EvolutionShellComponent methods.  */

static void
impl_owner_unset (EvolutionShellComponent *shell_component)
{
	EvolutionShellComponentPrivate *priv;

	priv = shell_component->priv;

	if (priv->ping_timeout_id != -1) {
		g_source_remove (priv->ping_timeout_id);
		priv->ping_timeout_id = -1;
	}

	g_object_unref (priv->owner_client);
	priv->owner_client = NULL;
}

static void
impl_owner_died (EvolutionShellComponent *shell_component)
{
	EvolutionShellComponentPrivate *priv;

	priv = shell_component->priv;

	g_object_unref (priv->owner_client);
	priv->owner_client = NULL;

	/* The default implementation for ::owner_died emits ::owner_unset, so
	   that we make the behavior for old components kind of correct without
	   even if they don't handle the new ::owner_died signal correctly
	   yet.  */

	g_signal_emit (shell_component, signals[OWNER_UNSET], 0);
}


/* Initialization.  */

static void
evolution_shell_component_class_init (EvolutionShellComponentClass *klass)
{
	EvolutionShellComponentClass *shell_component_class;
	GObjectClass *object_class;
	POA_GNOME_Evolution_ShellComponent__epv *epv = &klass->epv;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	signals[OWNER_SET]
		= g_signal_new ("owner_set",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionShellComponentClass, owner_set),
				NULL, NULL,
				e_shell_marshal_NONE__POINTER_POINTER,
				G_TYPE_NONE, 2,
				G_TYPE_POINTER, G_TYPE_POINTER);

	signals[OWNER_DIED]
		= g_signal_new ("owner_died",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionShellComponentClass, owner_died),
				NULL, NULL,
				e_shell_marshal_NONE__NONE,
				G_TYPE_NONE, 0);

	signals[OWNER_UNSET]
		= g_signal_new ("owner_unset",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionShellComponentClass, owner_unset),
				NULL, NULL,
				e_shell_marshal_NONE__NONE,
				G_TYPE_NONE, 0);

	signals[DEBUG]
		= g_signal_new ("debug",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionShellComponentClass, debug),
				NULL, NULL,
				e_shell_marshal_NONE__NONE,
				G_TYPE_NONE, 0);

	signals[INTERACTIVE]
		= g_signal_new ("interactive",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionShellComponentClass, interactive),
				NULL, NULL,
				e_shell_marshal_NONE__BOOL_INT,
				G_TYPE_NONE, 2,
				G_TYPE_BOOLEAN,
				G_TYPE_INT);

	signals[HANDLE_EXTERNAL_URI]
		= g_signal_new ("handle_external_uri",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionShellComponentClass, handle_external_uri),
				NULL, NULL,
				e_shell_marshal_NONE__STRING,
				G_TYPE_NONE, 1,
				G_TYPE_STRING);

	signals[USER_CREATE_NEW_ITEM]
		= g_signal_new ("user_create_new_item",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionShellComponentClass, user_create_new_item),
				NULL, NULL,
				e_shell_marshal_VOID__STRING_STRING_STRING,
				G_TYPE_NONE, 3,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING);

	signals[SEND_RECEIVE]
		= g_signal_new ("send_receive",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionShellComponentClass, send_receive),
				NULL, NULL,
				e_shell_marshal_NONE__BOOL,
				G_TYPE_NONE, 1,
				G_TYPE_BOOLEAN);

	parent_class = g_type_class_ref(PARENT_TYPE);

	epv->_get_supportedTypes         = impl__get_supportedTypes;
	epv->_get_externalUriSchemas     = impl__get_externalUriSchemas;
	epv->_get_userCreatableItemTypes = impl__get_userCreatableItemTypes;
	epv->setOwner                    = impl_setOwner; 
	epv->unsetOwner                  = impl_unsetOwner; 
	epv->debug                       = impl_debug;
	epv->interactive                 = impl_interactive;
	epv->createView                  = impl_createView; 
	epv->handleExternalURI           = impl_handleExternalURI; 
	epv->createFolderAsync           = impl_createFolderAsync; 
	epv->removeFolderAsync           = impl_removeFolderAsync; 
	epv->xferFolderAsync             = impl_xferFolderAsync; 
	epv->populateFolderContextMenu   = impl_populateFolderContextMenu;
	epv->unpopulateFolderContextMenu = impl_unpopulateFolderContextMenu;
	epv->userCreateNewItem           = impl_userCreateNewItem;
	epv->sendReceive                 = impl_sendReceive;
	epv->requestQuit                 = impl_requestQuit;

	shell_component_class = EVOLUTION_SHELL_COMPONENT_CLASS (object_class);
	shell_component_class->owner_died = impl_owner_died;
	shell_component_class->owner_unset = impl_owner_unset;
}

static void
evolution_shell_component_init (EvolutionShellComponent *shell_component)
{
	EvolutionShellComponentPrivate *priv;

	priv = g_new (EvolutionShellComponentPrivate, 1);

	priv->folder_types                    = NULL;
	priv->external_uri_schemas            = NULL;

	priv->create_view_fn                    = NULL;
	priv->create_folder_fn                  = NULL;
	priv->remove_folder_fn                  = NULL;
	priv->xfer_folder_fn                    = NULL;
	priv->populate_folder_context_menu_fn   = NULL;
	priv->unpopulate_folder_context_menu_fn = NULL;

	priv->owner_client                      = NULL;
	priv->user_creatable_item_types         = NULL;
	priv->closure                           = NULL;

	priv->ping_timeout_id                   = -1;

	priv->uic                               = NULL;

	shell_component->priv = priv;
}


void
evolution_shell_component_construct (EvolutionShellComponent *shell_component,
				     const EvolutionShellComponentFolderType folder_types[],
				     const char *external_uri_schemas[],
				     EvolutionShellComponentCreateViewFn create_view_fn,
				     EvolutionShellComponentCreateFolderFn create_folder_fn,
				     EvolutionShellComponentRemoveFolderFn remove_folder_fn,
				     EvolutionShellComponentXferFolderFn xfer_folder_fn,
				     EvolutionShellComponentPopulateFolderContextMenuFn populate_folder_context_menu_fn,
				     EvolutionShellComponentUnpopulateFolderContextMenuFn unpopulate_folder_context_menu_fn,
				     EvolutionShellComponentGetDndSelectionFn get_dnd_selection_fn,
				     EvolutionShellComponentRequestQuitFn request_quit_fn,
				     void *closure)
{
	EvolutionShellComponentPrivate *priv;
	int i;

	g_return_if_fail (shell_component != NULL);
	g_return_if_fail (EVOLUTION_IS_SHELL_COMPONENT (shell_component));
	g_return_if_fail (folder_types != NULL);

	priv = shell_component->priv;

	priv->create_view_fn                    = create_view_fn;
	priv->create_folder_fn                  = create_folder_fn;
	priv->remove_folder_fn                  = remove_folder_fn;
	priv->xfer_folder_fn                    = xfer_folder_fn;
	priv->populate_folder_context_menu_fn   = populate_folder_context_menu_fn;
	priv->unpopulate_folder_context_menu_fn = unpopulate_folder_context_menu_fn;
	priv->get_dnd_selection_fn              = get_dnd_selection_fn;
	priv->request_quit_fn                   = request_quit_fn;

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

		/* Notice that these get translated here.  */
		new->display_name       = g_strdup (_(folder_types[i].display_name));
		new->description        = g_strdup (_(folder_types[i].description));

		new->user_creatable     = folder_types[i].user_creatable;
		new->accepted_dnd_types = duplicate_null_terminated_string_array (folder_types[i].accepted_dnd_types);
		new->exported_dnd_types = duplicate_null_terminated_string_array (folder_types[i].exported_dnd_types);

		priv->folder_types = g_list_prepend (priv->folder_types, new);
	}

	if (priv->folder_types == NULL)
		g_warning ("No valid folder types constructing EShellComponent %p", shell_component);

	if (external_uri_schemas != NULL) {
		for (i = 0; external_uri_schemas[i] != NULL; i++)
			priv->external_uri_schemas = g_list_prepend (priv->external_uri_schemas,
								     g_strdup (external_uri_schemas[i]));
	}
}

EvolutionShellComponent *
evolution_shell_component_new (const EvolutionShellComponentFolderType folder_types[],
			       const char *external_uri_schemas[],
			       EvolutionShellComponentCreateViewFn create_view_fn,
			       EvolutionShellComponentCreateFolderFn create_folder_fn,
			       EvolutionShellComponentRemoveFolderFn remove_folder_fn,
			       EvolutionShellComponentXferFolderFn xfer_folder_fn,
			       EvolutionShellComponentPopulateFolderContextMenuFn populate_folder_context_menu_fn,
			       EvolutionShellComponentUnpopulateFolderContextMenuFn unpopulate_folder_context_menu_fn,
			       EvolutionShellComponentGetDndSelectionFn get_dnd_selection_fn,
			       EvolutionShellComponentRequestQuitFn request_quit_fn,
			       void *closure)
{
	EvolutionShellComponent *new;

	g_return_val_if_fail (folder_types != NULL, NULL);

	new = g_object_new (evolution_shell_component_get_type (), NULL);

	evolution_shell_component_construct (new,
					     folder_types,
					     external_uri_schemas,
					     create_view_fn,
					     create_folder_fn,
					     remove_folder_fn,
					     xfer_folder_fn,
					     populate_folder_context_menu_fn,
					     unpopulate_folder_context_menu_fn,
					     get_dnd_selection_fn,
					     request_quit_fn,
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

gulong evolution_shell_component_get_parent_view_xid(EvolutionShellComponent                            *shell_component)
{
	g_return_val_if_fail (shell_component != NULL, 0);
	g_return_val_if_fail (EVOLUTION_IS_SHELL_COMPONENT (shell_component), 0);

	return shell_component->priv->parent_view_xid;
}


void
evolution_shell_component_add_user_creatable_item  (EvolutionShellComponent *shell_component,
						    const char *id,
						    const char *description,
						    const char *menu_description,
						    const char *tooltip,
						    const char *folder_type,
						    char menu_shortcut,
						    GdkPixbuf *icon)
{
	EvolutionShellComponentPrivate *priv;
	UserCreatableItemType *type;

	g_return_if_fail (shell_component != NULL);
	g_return_if_fail (EVOLUTION_IS_SHELL_COMPONENT (shell_component));
	g_return_if_fail (id != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (menu_description != NULL);

	priv = shell_component->priv;

	type = user_creatable_item_type_new (id, description, menu_description, tooltip, folder_type, menu_shortcut, icon);

	priv->user_creatable_item_types = g_slist_prepend (priv->user_creatable_item_types, type);
}


/* Public utility functions.  */

const char *
evolution_shell_component_result_to_string (EvolutionShellComponentResult result)
{
	switch (result) {
	case EVOLUTION_SHELL_COMPONENT_OK:
		return _("Success");
	case EVOLUTION_SHELL_COMPONENT_CANCEL:
		return _("Cancel");
	case EVOLUTION_SHELL_COMPONENT_CORBAERROR:
		return _("CORBA error");
	case EVOLUTION_SHELL_COMPONENT_INTERRUPTED:
		return _("Interrupted");
	case EVOLUTION_SHELL_COMPONENT_INVALIDARG:
		return _("Invalid argument");
	case EVOLUTION_SHELL_COMPONENT_ALREADYOWNED:
		return _("Already has an owner");
	case EVOLUTION_SHELL_COMPONENT_NOTOWNED:
		return _("No owner");
	case EVOLUTION_SHELL_COMPONENT_NOTFOUND:
		return _("Not found");
	case EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE:
		return _("Unsupported type");
	case EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDSCHEMA:
		return _("Unsupported schema");
	case EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDOPERATION:
		return _("Unsupported operation");
	case EVOLUTION_SHELL_COMPONENT_INTERNALERROR:
		return _("Internal error");
	case EVOLUTION_SHELL_COMPONENT_BUSY:
		return _("Busy");
	case EVOLUTION_SHELL_COMPONENT_EXISTS:
		return _("Exists");
	case EVOLUTION_SHELL_COMPONENT_INVALIDURI:
		return _("Invalid URI");
	case EVOLUTION_SHELL_COMPONENT_PERMISSIONDENIED:
		return _("Permission denied");
	case EVOLUTION_SHELL_COMPONENT_HASSUBFOLDERS:
		return _("Has subfolders");
	case EVOLUTION_SHELL_COMPONENT_NOSPACE:
		return _("No space left");
	case EVOLUTION_SHELL_COMPONENT_OLDOWNERHASDIED:
		return _("Old owner has died");
	case EVOLUTION_SHELL_COMPONENT_UNKNOWNERROR:
	default:
		return _("Unknown error");
	}
}


BONOBO_TYPE_FUNC_FULL (EvolutionShellComponent,
		       GNOME_Evolution_ShellComponent,
		       PARENT_TYPE,
		       evolution_shell_component)
