/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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

#include <fcntl.h>

#include <glib.h>
#include <gtk/gtksignal.h>
#include <bonobo/bonobo-object.h>
#include <libgnome/gnome-i18n.h>

#include <gal/util/e-util.h>


#define PING_DELAY 10000


#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

static BonoboXObjectClass *parent_class = NULL;

struct _UserCreatableItemType {
	char *id;
	char *description;
	char *menu_description;
	char menu_shortcut;
	GdkPixbuf *icon;
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
	EvolutionShellComponentGetDndSelectionFn get_dnd_selection_fn;

	EvolutionShellClient *owner_client;

	GSList *user_creatable_item_types; /* UserCreatableItemType */

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
			      char menu_shortcut,
			      GdkPixbuf *icon)
{
	UserCreatableItemType *type;

	type = g_new (UserCreatableItemType, 1);
	type->id               = g_strdup (id);
	type->description      = g_strdup (description);
	type->menu_description = g_strdup (menu_description);
	type->menu_shortcut    = menu_shortcut;

	if (icon == NULL)
		type->icon = NULL;
	else
		type->icon = gdk_pixbuf_ref (icon);

	return type;
}

static void
user_creatable_item_type_free (UserCreatableItemType *type)
{
	g_free (type->id);
	g_free (type->description);
	g_free (type->menu_description);

	if (type->icon != NULL)
		gdk_pixbuf_unref (type->icon);

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

	owner_objref = bonobo_object_corba_objref (BONOBO_OBJECT (priv->owner_client));

	if (owner_objref == CORBA_OBJECT_NIL)
		return FALSE;

	/* We are duplicating the object here, as we might get an ::unsetOwner
	   while we invoke the pinging, and this would make the objref invalid
	   and thus crash the stubs (cfr. #13802).  */

	CORBA_exception_init (&ev);
	owner_objref = CORBA_Object_duplicate (owner_objref, &ev);

	alive = bonobo_unknown_ping (owner_objref);

	CORBA_Object_release (owner_objref, &ev);
	CORBA_exception_free (&ev);

	if (alive)
		return TRUE;

	/* This is tricky.  During the pinging, we might have gotten an
	   ::unsetOwner invocation which has invalidated our owner_client.  In
	   this case, no "owner_died" should be emitted.  */
	
	if (priv->owner_client != NULL) {
		g_print ("\t*** The shell has disappeared\n");
		gtk_signal_emit (GTK_OBJECT (shell_component), signals[OWNER_DIED]);
	}

	priv->ping_timeout_id = -1;

	return FALSE;
}

static void
setup_owner_pinging (EvolutionShellComponent *shell_component)
{
	EvolutionShellComponentPrivate *priv;

	priv = shell_component->priv;

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
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	GNOME_Evolution_Shell shell_duplicate;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);
	priv = shell_component->priv;

	if (priv->owner_client != NULL) {
		int owner_is_dead;

		owner_is_dead = CORBA_Object_non_existent
			(bonobo_object_corba_objref (BONOBO_OBJECT (priv->owner_client)), ev);
		if (ev->_major != CORBA_NO_EXCEPTION)
			owner_is_dead = TRUE;

		if (! owner_is_dead) {
			CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
					     ex_GNOME_Evolution_ShellComponent_AlreadyOwned, NULL);
		} else {
			CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
					     ex_GNOME_Evolution_ShellComponent_OldOwnerHasDied, NULL);

			gtk_signal_emit (GTK_OBJECT (shell_component), signals[OWNER_DIED]);
		}

		return;
	}

	shell_duplicate = CORBA_Object_duplicate (shell, ev);

	if (ev->_major == CORBA_NO_EXCEPTION) {
		priv->owner_client = evolution_shell_client_new (shell_duplicate);
		gtk_signal_emit (GTK_OBJECT (shell_component), signals[OWNER_SET], priv->owner_client, evolution_homedir);

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

	gtk_signal_emit (GTK_OBJECT (shell_component), signals[OWNER_UNSET]);
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

	gtk_signal_emit (GTK_OBJECT (shell_component), signals[DEBUG]);
}

static void
impl_interactive (PortableServer_Servant servant,
		  CORBA_boolean interactive,
		  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponent *shell_component;

	bonobo_object = bonobo_object_from_servant (servant);
	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object);

	gtk_signal_emit (GTK_OBJECT (shell_component), signals[INTERACTIVE], interactive);
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

	gtk_signal_emit (GTK_OBJECT (shell_component), signals[HANDLE_EXTERNAL_URI], uri);
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

	gtk_signal_emit (GTK_OBJECT (shell_component), signals[USER_CREATE_NEW_ITEM], id, parent_physical_uri, parent_type);
}

static void
impl_sendReceive (PortableServer_Servant servant,
		  const CORBA_boolean show_dialog,
		  CORBA_Environment *ev)
{
	EvolutionShellComponent *shell_component;

	shell_component = EVOLUTION_SHELL_COMPONENT (bonobo_object_from_servant (servant));
	gtk_signal_emit (GTK_OBJECT (shell_component), signals[SEND_RECEIVE], show_dialog);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentPrivate *priv;
	CORBA_Environment ev;
	GSList *sp;
	GList *p;

	shell_component = EVOLUTION_SHELL_COMPONENT (object);

	priv = shell_component->priv;

	if (priv->ping_timeout_id != -1) {
		g_source_remove (priv->ping_timeout_id);
		priv->ping_timeout_id = -1;
	}

	CORBA_exception_init (&ev);

	if (priv->owner_client != NULL) {
		BonoboObject *owner_client_object;

		owner_client_object = BONOBO_OBJECT (priv->owner_client);
		priv->owner_client = NULL;
		bonobo_object_unref (BONOBO_OBJECT (owner_client_object));
	}

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

	e_free_string_list (priv->external_uri_schemas);

	for (sp = priv->user_creatable_item_types; sp != NULL; sp = sp->next)
		user_creatable_item_type_free ((UserCreatableItemType *) sp->data);
	g_slist_free (priv->user_creatable_item_types);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* EvolutionShellComponent methods.  */

static void
impl_owner_unset (EvolutionShellComponent *shell_component)
{
	EvolutionShellComponentPrivate *priv;
	BonoboObject *owner_client_object;

	priv = shell_component->priv;

	if (priv->ping_timeout_id != -1) {
		g_source_remove (priv->ping_timeout_id);
		priv->ping_timeout_id = -1;
	}

	owner_client_object = BONOBO_OBJECT (priv->owner_client);
	priv->owner_client = NULL;
	bonobo_object_unref (BONOBO_OBJECT (owner_client_object));
}

static void
impl_owner_died (EvolutionShellComponent *shell_component)
{
	EvolutionShellComponentPrivate *priv;
	BonoboObject *owner_client_object;

	priv = shell_component->priv;

	owner_client_object = BONOBO_OBJECT (priv->owner_client);
	priv->owner_client = NULL;
	bonobo_object_unref (BONOBO_OBJECT (owner_client_object));

	/* The default implementation for ::owner_died emits ::owner_unset, so
	   that we make the behavior for old components kind of correct without
	   even if they don't handle the new ::owner_died signal correctly
	   yet.  */

	gtk_signal_emit (GTK_OBJECT (shell_component), signals[OWNER_UNSET]);
}


/* Initialization.  */

static void
class_init (EvolutionShellComponentClass *klass)
{
	EvolutionShellComponentClass *shell_component_class;
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

	signals[OWNER_DIED]
		= gtk_signal_new ("owner_died",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionShellComponentClass, owner_died),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

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

	signals[INTERACTIVE]
		= gtk_signal_new ("interactive",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionShellComponentClass, interactive),
				  gtk_marshal_NONE__BOOL,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_BOOL);

	signals[HANDLE_EXTERNAL_URI]
		= gtk_signal_new ("handle_external_uri",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionShellComponentClass, handle_external_uri),
				  gtk_marshal_NONE__STRING,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_STRING);

	signals[USER_CREATE_NEW_ITEM]
		= gtk_signal_new ("user_create_new_item",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionShellComponentClass, user_create_new_item),
				  gtk_marshal_NONE__POINTER_POINTER_POINTER,
				  GTK_TYPE_NONE, 3,
				  GTK_TYPE_STRING,
				  GTK_TYPE_STRING,
				  GTK_TYPE_STRING);

	signals[SEND_RECEIVE]
		= gtk_signal_new ("send_receive",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionShellComponentClass, send_receive),
				  gtk_marshal_NONE__BOOL,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_BOOL);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	parent_class = gtk_type_class (PARENT_TYPE);

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
	epv->userCreateNewItem           = impl_userCreateNewItem;
	epv->sendReceive                 = impl_sendReceive;

	shell_component_class = EVOLUTION_SHELL_COMPONENT_CLASS (object_class);
	shell_component_class->owner_died = impl_owner_died;
	shell_component_class->owner_unset = impl_owner_unset;
}

static void
init (EvolutionShellComponent *shell_component)
{
	EvolutionShellComponentPrivate *priv;

	priv = g_new (EvolutionShellComponentPrivate, 1);

	priv->folder_types                    = NULL;
	priv->external_uri_schemas            = NULL;

	priv->create_view_fn                  = NULL;
	priv->create_folder_fn                = NULL;
	priv->remove_folder_fn                = NULL;
	priv->xfer_folder_fn                  = NULL;
	priv->populate_folder_context_menu_fn = NULL;

	priv->owner_client                    = NULL;
	priv->user_creatable_item_types       = NULL;
	priv->closure                         = NULL;

	priv->ping_timeout_id                 = -1;

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
				     EvolutionShellComponentGetDndSelectionFn get_dnd_selection_fn,
				     void *closure)
{
	EvolutionShellComponentPrivate *priv;
	int i;

	g_return_if_fail (shell_component != NULL);
	g_return_if_fail (EVOLUTION_IS_SHELL_COMPONENT (shell_component));
	g_return_if_fail (folder_types != NULL);

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
			       EvolutionShellComponentGetDndSelectionFn get_dnd_selection_fn,
			       void *closure)
{
	EvolutionShellComponent *new;

	g_return_val_if_fail (folder_types != NULL, NULL);

	new = gtk_type_new (evolution_shell_component_get_type ());

	evolution_shell_component_construct (new,
					     folder_types,
					     external_uri_schemas,
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


void
evolution_shell_component_add_user_creatable_item  (EvolutionShellComponent *shell_component,
						    const char *id,
						    const char *description,
						    const char *menu_description,
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

	type = user_creatable_item_type_new (id, description, menu_description, menu_shortcut, icon);

	priv->user_creatable_item_types = g_slist_prepend (priv->user_creatable_item_types, type);
}


/* Public utility functions.  */

const char *
evolution_shell_component_result_to_string (EvolutionShellComponentResult result)
{
	switch (result) {
	case EVOLUTION_SHELL_COMPONENT_OK:
		return _("Success");
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


E_MAKE_X_TYPE (evolution_shell_component, "EvolutionShellComponent", EvolutionShellComponent,
	       class_init, init, PARENT_TYPE,
	       POA_GNOME_Evolution_ShellComponent__init,
	       GTK_STRUCT_OFFSET (EvolutionShellComponentClass, epv))
