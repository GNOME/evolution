/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell.c
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

#include <gnome.h>
#include <liboaf/liboaf.h>

#include "Evolution.h"

#include <gal/widgets/e-gui-utils.h>
#include <gal/util/e-util.h>

#include "e-component-registry.h"
#include "e-corba-storage-registry.h"
#include "e-folder-type-registry.h"
#include "e-local-storage.h"
#include "e-shell-constants.h"
#include "e-shell-folder-selection-dialog.h"
#include "e-shell-view.h"
#include "e-shortcuts.h"
#include "e-storage-set.h"
#include "e-splash.h"

#include "evolution-storage-set-view-factory.h"

#include "e-shell.h"


#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _EShellPrivate {
	char *local_directory;

	GList *views;

	EStorageSet *storage_set;
	ELocalStorage *local_storage;

	EShortcuts *shortcuts;
	EFolderTypeRegistry *folder_type_registry;

	EComponentRegistry *component_registry;

	ECorbaStorageRegistry *corba_storage_registry;

	/* Names for the types of the folders that have maybe crashed.  */
	GList *crash_type_names; /* char * */
};


/* Constants.  */

/* FIXME: We need a component repository instead.  */

#define SHORTCUTS_FILE_NAME     "shortcuts.xml"
#define LOCAL_STORAGE_DIRECTORY "local"


enum {
	NO_VIEWS_LEFT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* Callback for the folder selection dialog.  */

static void
folder_selection_dialog_cancelled_cb (EShellFolderSelectionDialog *folder_selection_dialog,
				      void *data)
{
	GNOME_Evolution_FolderSelectionListener listener;
	CORBA_Environment ev;

	listener = gtk_object_get_data (GTK_OBJECT (folder_selection_dialog), "corba_listener");

	CORBA_exception_init (&ev);

	GNOME_Evolution_FolderSelectionListener_notifyCanceled (listener, &ev);

	CORBA_exception_free (&ev);

	gtk_widget_destroy (GTK_WIDGET (folder_selection_dialog));
}

static void
folder_selection_dialog_folder_selected_cb (EShellFolderSelectionDialog *folder_selection_dialog,
					    const char *path,
					    void *data)
{
	CORBA_Environment ev;
	EShell *shell;
	GNOME_Evolution_FolderSelectionListener listener;
	EStorageSet *storage_set;
	EFolder *folder;
	char *uri;
	const char *physical_uri;

	shell = E_SHELL (data);
	listener = gtk_object_get_data (GTK_OBJECT (folder_selection_dialog), "corba_listener");

	CORBA_exception_init (&ev);

	storage_set = e_shell_get_storage_set (shell);
	folder = e_storage_set_get_folder (storage_set, path);

	uri = g_strconcat (E_SHELL_URI_PREFIX, path, NULL);

	if (folder == NULL)
		physical_uri = "";
	else
		physical_uri = e_folder_get_physical_uri (folder);

	GNOME_Evolution_FolderSelectionListener_notifySelected (listener, uri, physical_uri, &ev);
	g_free (uri);

	CORBA_exception_free (&ev);

	gtk_widget_destroy (GTK_WIDGET (folder_selection_dialog));
}


/* CORBA interface implementation.  */

static POA_GNOME_Evolution_Shell__vepv shell_vepv;

static POA_GNOME_Evolution_Shell *
create_servant (void)
{
	POA_GNOME_Evolution_Shell *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_Evolution_Shell *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &shell_vepv;

	CORBA_exception_init (&ev);

	POA_GNOME_Evolution_Shell__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static GNOME_Evolution_ShellComponent
impl_Shell_getComponentByType (PortableServer_Servant servant,
			       const CORBA_char *type,
			       CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponentClient *handler;
	EFolderTypeRegistry *folder_type_registry;
	GNOME_Evolution_ShellComponent corba_component;
	EShell *shell;

	bonobo_object = bonobo_object_from_servant (servant);
	shell = E_SHELL (bonobo_object);
	folder_type_registry = shell->priv->folder_type_registry;

	handler = e_folder_type_registry_get_handler_for_type (folder_type_registry, type);

	if (handler == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Shell_NotFound, NULL);
		return CORBA_OBJECT_NIL;
	}

	corba_component = bonobo_object_corba_objref (BONOBO_OBJECT (handler));
	Bonobo_Unknown_ref (corba_component, ev);

	return CORBA_Object_duplicate (corba_component, ev);
}

static GNOME_Evolution_ShellView
impl_Shell_createNewView (PortableServer_Servant servant,
			  const CORBA_char *uri,
			  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EShell *shell;
	EShellView *shell_view;
	GNOME_Evolution_ShellView shell_view_interface;

	bonobo_object = bonobo_object_from_servant (servant);
	shell = E_SHELL (bonobo_object);

	shell_view = e_shell_new_view (shell, uri);
	shell_view_interface = e_shell_view_get_corba_interface (shell_view);

	Bonobo_Unknown_ref (shell_view_interface, ev);
	return CORBA_Object_duplicate ((CORBA_Object) shell_view_interface, ev);
}

static void
corba_listener_destroy_notify (void *data)
{
	CORBA_Environment ev;
	GNOME_Evolution_FolderSelectionListener listener_interface;

	listener_interface = (GNOME_Evolution_FolderSelectionListener) data;

	CORBA_exception_init (&ev);
	CORBA_Object_release (listener_interface, &ev);
	CORBA_exception_free (&ev);
}

static void
impl_Shell_selectUserFolder (PortableServer_Servant servant,
			     const GNOME_Evolution_FolderSelectionListener listener,
			     const CORBA_char *title,
			     const CORBA_char *default_folder,
			     const GNOME_Evolution_Shell_FolderTypeNameList *corba_allowed_type_names,
			     CORBA_Environment *ev)
{
	GtkWidget *folder_selection_dialog;
	BonoboObject *bonobo_object;
	GNOME_Evolution_FolderSelectionListener listener_duplicate;
	EShell *shell;
	const char **allowed_type_names;
	int i;

	bonobo_object = bonobo_object_from_servant (servant);
	shell = E_SHELL (bonobo_object);

	allowed_type_names = alloca (sizeof (allowed_type_names[0]) * (corba_allowed_type_names->_length + 1));
	for (i = 0; i < corba_allowed_type_names->_length; i++)
		allowed_type_names[i] = corba_allowed_type_names->_buffer[i];
	allowed_type_names[corba_allowed_type_names->_length] = NULL;

	/* CORBA doesn't allow you to pass a NULL pointer. */
	if (!*default_folder)
		default_folder = NULL;
	folder_selection_dialog = e_shell_folder_selection_dialog_new (shell, title, default_folder, allowed_type_names);

	listener_duplicate = CORBA_Object_duplicate (listener, ev);
	gtk_object_set_data_full (GTK_OBJECT (folder_selection_dialog), "corba_listener",
				  listener_duplicate, corba_listener_destroy_notify);

	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "folder_selected",
			    GTK_SIGNAL_FUNC (folder_selection_dialog_folder_selected_cb), shell);
	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "cancelled",
			    GTK_SIGNAL_FUNC (folder_selection_dialog_cancelled_cb), shell);

	gtk_widget_show (folder_selection_dialog);
}

static GNOME_Evolution_LocalStorage
impl_Shell_getLocalStorage (PortableServer_Servant servant,
			    CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	GNOME_Evolution_LocalStorage local_storage_interface;
	GNOME_Evolution_LocalStorage copy_of_local_storage_interface;
	EShell *shell;
	EShellPrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	shell = E_SHELL (bonobo_object);
	priv = shell->priv;

	local_storage_interface = e_local_storage_get_corba_interface (priv->local_storage);

	copy_of_local_storage_interface = CORBA_Object_duplicate (local_storage_interface, ev);
	Bonobo_Unknown_ref (copy_of_local_storage_interface, ev);

	return copy_of_local_storage_interface;
}

static Bonobo_Control
impl_Shell_createStorageSetView (PortableServer_Servant servant,
				 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EShell *shell;
	BonoboControl *control;

	bonobo_object = bonobo_object_from_servant (servant);
	shell = E_SHELL (bonobo_object);

	control = evolution_storage_set_view_factory_new_view (shell);

	return bonobo_object_corba_objref (BONOBO_OBJECT (control));
}


/* OAF registration.  */

static OAF_RegistrationResult
register_shell (EShell *shell,
		const char *iid)
{
	CORBA_Object corba_object;

	/* FIXME: Multi-display stuff.  */

	corba_object = bonobo_object_corba_objref (BONOBO_OBJECT (shell));
	return oaf_active_server_register (iid, corba_object);
}


/* Initialization of the storages.  */

static gboolean
setup_corba_storages (EShell *shell)
{
	EShellPrivate *priv;
	ECorbaStorageRegistry *corba_storage_registry;

	priv = shell->priv;

	g_assert (priv->storage_set != NULL);
	corba_storage_registry = e_corba_storage_registry_new (priv->storage_set);

	if (corba_storage_registry == NULL)
		return FALSE;

	bonobo_object_add_interface (BONOBO_OBJECT (shell), BONOBO_OBJECT (corba_storage_registry));

	/* Notice that `bonobo_object_add_interface()' aggregates the two object's
           reference counts, so we need an extra ref here if we want to keep a separate
           pointer to the storage interface.  */
	bonobo_object_ref (BONOBO_OBJECT (corba_storage_registry));
	priv->corba_storage_registry = corba_storage_registry;

	return TRUE;
}

static gboolean
setup_local_storage (EShell *shell)
{
	EStorage *local_storage;
	EShellPrivate *priv;
	gchar *local_storage_path;

	priv = shell->priv;

	g_assert (priv->folder_type_registry != NULL);
	g_assert (priv->local_storage == NULL);

	local_storage_path = g_concat_dir_and_file (priv->local_directory, LOCAL_STORAGE_DIRECTORY);
	local_storage = e_local_storage_open (priv->folder_type_registry, local_storage_path);
	if (local_storage == NULL) {
		g_warning (_("Cannot set up local storage -- %s"), local_storage_path);
		g_free (local_storage_path);
		return FALSE;
	}
	g_free (local_storage_path);

	e_storage_set_add_storage (priv->storage_set, local_storage);
	priv->local_storage = E_LOCAL_STORAGE (local_storage);

	return TRUE;
}


/* Initialization of the components.  */

static char *
get_icon_path_for_component_info (const OAF_ServerInfo *info)
{
	OAF_Property *property;
	const char *shell_component_icon_value;

	/* FIXME: liboaf is not const-safe.  */
	property = oaf_server_info_prop_find ((OAF_ServerInfo *) info,
					      "evolution:shell-component-icon");

	if (property == NULL || property->v._d != OAF_P_STRING)
		return gnome_pixmap_file ("gnome-question.png");

	shell_component_icon_value = property->v._u.value_string;

	if (g_path_is_absolute (shell_component_icon_value))
		return g_strdup (shell_component_icon_value);

	else
		return g_concat_dir_and_file (EVOLUTION_IMAGES, shell_component_icon_value);
}

static void
setup_components (EShell *shell,
		  ESplash *splash)
{
	EShellPrivate *priv;
	OAF_ServerInfoList *info_list;
	CORBA_Environment ev;
	int i;

	CORBA_exception_init (&ev);

	priv = shell->priv;
	priv->component_registry = e_component_registry_new (shell);

	info_list = oaf_query ("repo_ids.has ('IDL:GNOME/Evolution/ShellComponent:1.0')", NULL, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_error ("Eeek!  Cannot perform OAF query for Evolution components.");

	for (i = 0; i < info_list->_length; i++) {
		const OAF_ServerInfo *info;
		GdkPixbuf *icon_pixbuf;
		char *icon_path;

		info = info_list->_buffer + i;

		icon_path = get_icon_path_for_component_info (info);

		icon_pixbuf = gdk_pixbuf_new_from_file (icon_path);

		if (splash != NULL)
			e_splash_add_icon (splash, icon_pixbuf);

		gdk_pixbuf_unref (icon_pixbuf);

		g_free (icon_path);
	}

	while (gtk_events_pending ())
		gtk_main_iteration ();

	for (i = 0; i < info_list->_length; i++) {
		const OAF_ServerInfo *info;

		info = info_list->_buffer + i;

		if (! e_component_registry_register_component (priv->component_registry, info->iid))
			g_warning ("Cannot activate Evolution component -- %s", info->iid);
		else
			g_print ("Evolution component activated successfully -- %s\n", info->iid);

		if (splash != NULL)
			e_splash_set_icon_highlight (splash, i, TRUE);

		while (gtk_events_pending ())
			gtk_main_iteration ();
	}

	if (info_list->_length == 0)
		g_warning ("No Evolution components installed.");

	CORBA_free (info_list);

	CORBA_exception_free (&ev);
}

/* FIXME what if anything fails here?  */
static void
set_owner_on_components (EShell *shell)
{
	GNOME_Evolution_Shell corba_shell;
	EShellPrivate *priv;
	const char *local_directory;
	GList *id_list;
	GList *p;

	priv = shell->priv;
	local_directory = e_shell_get_local_directory (shell);

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell));

	id_list = e_component_registry_get_id_list (priv->component_registry);
	for (p = id_list; p != NULL; p = p->next) {
		EvolutionShellComponentClient *component_client;
		const char *id;

		id = (const char *) p->data;
		component_client = e_component_registry_get_component_by_id (priv->component_registry, id);

		evolution_shell_component_client_set_owner (component_client, corba_shell, local_directory);
	}

	e_free_string_list (id_list);
}


/* EShellView destruction callback.  */

static int
view_deleted_cb (GtkObject *object,
		 GdkEvent *ev,
		 gpointer data)
{
	EShell *shell;

	g_assert (E_IS_SHELL_VIEW (object));

	shell = E_SHELL (data);
	e_shell_save_settings (shell);

	/* Destroy it */
	return FALSE;
}

static void
view_destroy_cb (GtkObject *object,
		 gpointer data)
{
	EShell *shell;
	int nviews;

	g_assert (E_IS_SHELL_VIEW (object));

	shell = E_SHELL (data);

	nviews = g_list_length (shell->priv->views);

	/* If this is our last view, save settings now because in the
	   callback for no_views_left shell->priv->views will be NULL
	   and settings won't be saved because of that */
	if (nviews - 1 == 0)
		e_shell_save_settings (shell);

	shell->priv->views = g_list_remove (shell->priv->views, object);

	if (shell->priv->views == NULL) {
		/* FIXME: This looks like a Bonobo bug to me.  */
		bonobo_object_ref (BONOBO_OBJECT (shell));
		gtk_signal_emit (GTK_OBJECT (shell), signals [NO_VIEWS_LEFT]);
		bonobo_object_unref (BONOBO_OBJECT (shell));
	}
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EShell *shell;
	EShellPrivate *priv;
	GList *p;

	shell = E_SHELL (object);
	priv = shell->priv;

	g_free (priv->local_directory);

	if (priv->storage_set != NULL)
		gtk_object_unref (GTK_OBJECT (priv->storage_set));

	if (priv->local_storage != NULL)
		gtk_object_unref (GTK_OBJECT (priv->local_storage));

	if (priv->shortcuts != NULL)
		gtk_object_unref (GTK_OBJECT (priv->shortcuts));

	if (priv->folder_type_registry != NULL)
		gtk_object_unref (GTK_OBJECT (priv->folder_type_registry));

	if (priv->component_registry != NULL)
		gtk_object_unref (GTK_OBJECT (priv->component_registry));

	for (p = priv->views; p != NULL; p = p->next) {
		EShellView *view;

		view = E_SHELL_VIEW (p->data);

		gtk_signal_disconnect_by_func (
			GTK_OBJECT (view),
			GTK_SIGNAL_FUNC (view_destroy_cb), shell);
		gtk_signal_disconnect_by_func (GTK_OBJECT (view),
					       GTK_SIGNAL_FUNC (view_deleted_cb),
					       shell);

		gtk_object_destroy (GTK_OBJECT (view));
	}

	g_list_free (priv->views);

	if (priv->corba_storage_registry != NULL)
		bonobo_object_unref (BONOBO_OBJECT (priv->corba_storage_registry));

	e_free_string_list (priv->crash_type_names);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* Initialization.  */

static void
corba_class_init (void)
{
	POA_GNOME_Evolution_Shell__vepv *vepv;
	POA_GNOME_Evolution_Shell__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_GNOME_Evolution_Shell__epv, 1);
	epv->getComponentByType   = impl_Shell_getComponentByType;
	epv->createNewView        = impl_Shell_createNewView;
	epv->selectUserFolder     = impl_Shell_selectUserFolder;
	epv->getLocalStorage      = impl_Shell_getLocalStorage;
	epv->createStorageSetView = impl_Shell_createStorageSetView;

	vepv = &shell_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_Shell_epv = epv;
}

static void
class_init (EShellClass *klass)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	signals[NO_VIEWS_LEFT] =
		gtk_signal_new ("no_views_left",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EShellClass, no_views_left),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	corba_class_init ();
}

static void
init (EShell *shell)
{
	EShellPrivate *priv;

	priv = g_new (EShellPrivate, 1);

	priv->views = NULL;

	priv->local_directory        = NULL;
	priv->storage_set            = NULL;
	priv->local_storage          = NULL;
	priv->shortcuts              = NULL;
	priv->component_registry     = NULL;
	priv->folder_type_registry   = NULL;
	priv->corba_storage_registry = NULL;
	priv->crash_type_names       = NULL;

	shell->priv = priv;
}


/**
 * e_shell_construct:
 * @shell: An EShell object to construct
 * @corba_object: A CORBA Object implementing the Evolution::Shell interface
 * @iid: OAFIID for registering the shell into the name server
 * @local_directory: Local directory for storing local information and folders
 * @show_splash: Whether to display a splash screen.
 * 
 * Construct @shell so that it uses the specified @local_directory and
 * @corba_object.
 *
 * Return value: %FALSE if the shell cannot be registered; %TRUE otherwise.
 **/
gboolean
e_shell_construct (EShell *shell,
		   GNOME_Evolution_Shell corba_object,
		   const char *iid,
		   const char *local_directory,
		   gboolean show_splash)
{
	GtkWidget *splash;
	EShellPrivate *priv;
	gchar *shortcut_path;

	g_return_val_if_fail (shell != NULL, FALSE);
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, FALSE);
	g_return_val_if_fail (local_directory != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (local_directory), FALSE);

	bonobo_object_construct (BONOBO_OBJECT (shell), corba_object);

	if (register_shell (shell, iid) != OAF_REG_SUCCESS)
		return FALSE;

	if (! show_splash) {
		splash = NULL;
	} else {
		splash = e_splash_new ();
		gtk_widget_show (splash);

		/* Keep our own reference */
		gtk_object_ref (GTK_OBJECT (splash));
	}

	while (gtk_events_pending ())
		gtk_main_iteration ();

	priv = shell->priv;

	priv->local_directory      = g_strdup (local_directory);
	priv->folder_type_registry = e_folder_type_registry_new ();
	priv->storage_set          = e_storage_set_new (shell->priv->folder_type_registry);

	gtk_object_ref (GTK_OBJECT (priv->folder_type_registry));
	gtk_object_ref (GTK_OBJECT (priv->storage_set));

	/* CORBA storages must be set up before the components, because otherwise components
           cannot register their own storages.  */
	if (! setup_corba_storages (shell))
		return FALSE;

	if (splash != NULL)
		setup_components (shell, E_SPLASH (splash));
	else
		setup_components (shell, NULL);

	/* The local storage depends on the component registry.  */
	setup_local_storage (shell);

	/* Now that we have a local storage, we can tell the components we are here.  */
	set_owner_on_components (shell);

	/* Run the intelligent importers to find see if any data needs 
	   importing. */
	intelligent_importer_init ();

	shortcut_path = g_concat_dir_and_file (local_directory, "shortcuts.xml");
	priv->shortcuts = e_shortcuts_new (priv->storage_set,
					   priv->folder_type_registry,
					   shortcut_path);

	if (priv->shortcuts == NULL)
		g_warning ("Cannot load shortcuts -- %s", shortcut_path);
	else
		gtk_object_ref (GTK_OBJECT (priv->shortcuts));

	g_free (shortcut_path);

	sleep (2);
	gtk_widget_unref (splash);
	gtk_widget_destroy (splash);

	return TRUE;
}

/**
 * e_shell_new:
 * @local_directory: Local directory for storing local information and folders.
 * @show_splash: Whether to display a splash screen.
 * 
 * Create a new EShell.
 * 
 * Return value: 
 **/
EShell *
e_shell_new (const char *local_directory,
	     gboolean show_splash)
{
	EShell *new;
	EShellPrivate *priv;
	GNOME_Evolution_Shell corba_object;
	POA_GNOME_Evolution_Shell *servant;

	g_return_val_if_fail (local_directory != NULL, NULL);
	g_return_val_if_fail (*local_directory != '\0', NULL);

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	new = gtk_type_new (e_shell_get_type ());

	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (new), servant);
	if (! e_shell_construct (new,
				 corba_object, E_SHELL_OAFIID,
				 local_directory, show_splash)) {
		bonobo_object_unref (BONOBO_OBJECT (new));
		return NULL;
	}

	priv = new->priv;

	if (priv->shortcuts == NULL || priv->storage_set == NULL) {
		bonobo_object_unref (BONOBO_OBJECT (new));
		return NULL;
	}

	return new;
}


/**
 * e_shell_new_view:
 * @shell: The shell for which to create a new view.
 * @uri: URI for the new view.
 * 
 * Create a new view for @uri.
 * 
 * Return value: The new view.
 **/
EShellView *
e_shell_new_view (EShell *shell,
		  const char *uri)
{
	EShellView *view;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	view = e_shell_view_new (shell);

	gtk_widget_show (GTK_WIDGET (view));
	gtk_signal_connect (GTK_OBJECT (view), "delete-event",
			    GTK_SIGNAL_FUNC (view_deleted_cb), shell);
	gtk_signal_connect (GTK_OBJECT (view), "destroy",
			    GTK_SIGNAL_FUNC (view_destroy_cb), shell);

	if (uri != NULL)
		e_shell_view_display_uri (E_SHELL_VIEW (view), uri);

	shell->priv->views = g_list_prepend (shell->priv->views, view);

	return view;
}


/**
 * e_shell_get_local_directory:
 * @shell: An EShell object.
 * 
 * Get the local directory associated with @shell.
 * 
 * Return value: A pointer to the path of the local directory.
 **/
const char *
e_shell_get_local_directory (EShell *shell)
{
	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->local_directory;
}

/**
 * e_shell_get_shortcuts:
 * @shell: An EShell object.
 * 
 * Get the shortcuts associated to @shell.
 * 
 * Return value: A pointer to the EShortcuts associated to @shell.
 **/
EShortcuts *
e_shell_get_shortcuts (EShell *shell)
{
	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->shortcuts;
}

/**
 * e_shell_get_storage_set:
 * @shell: An EShell object.
 * 
 * Get the storage set associated to @shell.
 * 
 * Return value: A pointer to the EStorageSet associated to @shell.
 **/
EStorageSet *
e_shell_get_storage_set (EShell *shell)
{
	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->storage_set;
}

/**
 * e_shell_get_folder_type_registry:
 * @shell: An EShell object.
 * 
 * Get the folder type registry associated to @shell.
 * 
 * Return value: A pointer to the EFolderTypeRegistry associated to @shell.
 **/
EFolderTypeRegistry *
e_shell_get_folder_type_registry (EShell *shell)
{
	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->folder_type_registry;
}

/**
 * e_shell_get_local_storage:
 * @shell: An EShell object.
 *
 * Get the local storage associated to @shell.
 *
 * Return value: A pointer to the ELocalStorage associated to @shell.
 **/
ELocalStorage *
e_shell_get_local_storage (EShell *shell)
{
	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->local_storage;
}


static gboolean
save_settings_for_views (EShell *shell)
{
	EShellPrivate *priv;
	GList *p;
	gboolean retval;
	char *prefix;
	int i;

	priv = shell->priv;
	retval = TRUE;

	for (p = priv->views, i = 0; p != NULL; p = p->next, i++) {
		EShellView *view;

		view = E_SHELL_VIEW (p->data);

		prefix = g_strdup_printf ("=%s/config/Shell=/Views/%d/",
					  priv->local_directory, i);

		if (! e_shell_view_save_settings (view, prefix)) {
			g_warning ("Cannot save settings for view -- %d", i);
			retval = FALSE;
		}

		g_free (prefix);
	}

	prefix = g_strdup_printf ("=%s/config/Shell=/Views/NumberOfViews",
				  priv->local_directory);
	gnome_config_set_int (prefix, g_list_length (priv->views));
	g_free (prefix);
	
	gnome_config_sync ();

	return TRUE;
}

static gboolean
save_settings_for_component (EShell *shell,
			     const char *id,
			     EvolutionShellComponentClient *client)
{
	Bonobo_Unknown unknown_interface;
	GNOME_Evolution_Session session_interface;
	CORBA_Environment ev;
	char *prefix;
	gboolean retval;

	unknown_interface = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	g_assert (unknown_interface != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	session_interface = Bonobo_Unknown_queryInterface (unknown_interface, "IDL:GNOME/Evolution/Session:1.0", &ev);
	if (ev._major != CORBA_NO_EXCEPTION || CORBA_Object_is_nil (session_interface, &ev)) {
		CORBA_exception_free (&ev);
		return TRUE;
	}

	prefix = g_strconcat ("/apps/Evolution/Shell/Components/", id, NULL);
	GNOME_Evolution_Session_saveConfiguration (session_interface, prefix, &ev);

	if (ev._major == CORBA_NO_EXCEPTION)
		retval = TRUE;
	else
		retval = FALSE;

	g_free (prefix);

	CORBA_exception_free (&ev);

	return retval;
}

static gboolean
save_settings_for_components (EShell *shell)
{
	EShellPrivate *priv;
	GList *component_ids;
	GList *p;
	gboolean retval;

	priv = shell->priv;

	g_assert (priv->component_registry);
	component_ids = e_component_registry_get_id_list (priv->component_registry);

	retval = TRUE;
	for (p = component_ids; p != NULL; p = p->next) {
		EvolutionShellComponentClient *client;
		const char *id;

		id = p->data;
		client = e_component_registry_get_component_by_id (priv->component_registry, id);

		if (! save_settings_for_component (shell, id, client))
			retval = FALSE;
	}

	e_free_string_list (component_ids);

	return retval;
}

/**
 * e_shell_save_settings:
 * @shell: 
 * 
 * Save the settings for this shell.
 * 
 * Return value: %TRUE if it worked, %FALSE otherwise.  Even if %FALSE is
 * returned, it is possible that at least part of the settings for the views
 * have been saved.
 **/
gboolean
e_shell_save_settings (EShell *shell)
{
	gboolean views_saved;
	gboolean components_saved;

	g_return_val_if_fail (shell != NULL, FALSE);
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	views_saved      = save_settings_for_views (shell);
	components_saved = save_settings_for_components (shell);

	return views_saved && components_saved;
}

/**
 * e_shell_restore_from_settings:
 * @shell: An EShell object.
 * 
 * Restore the existing views from the saved configuration.  The shell must
 * have no views for this to work.
 * 
 * Return value: %FALSE if the shell has some open views or there is no saved
 * configuration.  %TRUE if the configuration could be restored successfully.
 **/
gboolean
e_shell_restore_from_settings (EShell *shell)
{
	EShellPrivate *priv;
	gboolean retval;
	char *prefix;
	int num_views;
	int i;

	g_return_val_if_fail (shell != NULL, FALSE);
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);
	g_return_val_if_fail (shell->priv->views == NULL, FALSE);

	priv = shell->priv;

	prefix = g_strdup_printf ("=%s/config/Shell=/Views/NumberOfViews",
				  priv->local_directory);
	num_views = gnome_config_get_int (prefix);
	g_free (prefix);

	if (num_views == 0)
		return FALSE;
	
	retval = TRUE;

	for (i = 0; i < num_views; i++) {
		EShellView *view;

		prefix = g_strdup_printf ("=%s/config/Shell=/Views/%d/",
					  priv->local_directory, i);

		/* FIXME restore the URI here.  There should be an
                   e_shell_view_new_from_configuration() thingie.  */
		view = e_shell_new_view (shell, NULL);

		if (! e_shell_view_load_settings (view, prefix))
			retval = FALSE;

		g_free (prefix);
	}

	return retval;
}

/**
 * e_shell_quit:
 * @shell: An EShell.
 * 
 * Make @shell quit.  This will close all the associated views and destroy the
 * object.
 **/
void
e_shell_quit (EShell *shell)
{
	EShellPrivate *priv;
	GList *p;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	if (shell->priv->views)
		e_shell_save_settings (shell); 

	priv = shell->priv;

	for (p = priv->views; p != NULL; p = p->next) {
		EShellView *shell_view;

		shell_view = E_SHELL_VIEW (p->data);
		gtk_signal_disconnect_by_func (
			GTK_OBJECT (shell_view),
			GTK_SIGNAL_FUNC (view_destroy_cb), shell);
		gtk_object_destroy (GTK_OBJECT (shell_view));
	}

	g_list_free (priv->views);
	priv->views = NULL;

	bonobo_object_unref (BONOBO_OBJECT (priv->corba_storage_registry));

	priv->corba_storage_registry = NULL;

	e_storage_set_remove_all_storages (priv->storage_set);

	/*
	 *  Ok, so you thought the GUI components lifecycle was coupled to
	 * the Shell's, in fact this is not the case, they are unref'd
	 * here, and NULL'd to avoid shell destruction killing them again.
	 * So; the shell can be destroyed either remotely or localy.
	 */

	gtk_object_unref (GTK_OBJECT (priv->storage_set));
	gtk_object_unref (GTK_OBJECT (priv->shortcuts));
	gtk_object_unref (GTK_OBJECT (priv->folder_type_registry));
	gtk_object_unref (GTK_OBJECT (priv->component_registry));

	priv->storage_set          = NULL;
	priv->shortcuts            = NULL;
	priv->folder_type_registry = NULL;
	priv->component_registry   = NULL;

	bonobo_object_unref (BONOBO_OBJECT (shell));
}


/**
 * e_shell_component_maybe_crashed:
 * @shell: A pointer to an EShell object
 * @uri: URI that caused the crash
 * @type_name: The type of the folder that caused the crash
 * @shell_view: Pointer to the EShellView over which we want the modal dialog
 * to appear.
 * 
 * Report that a maybe crash happened when trying to display a folder of type
 * @type_name.  The shell will pop up a crash dialog whose parent will be the
 * @shell_view.
 **/
void
e_shell_component_maybe_crashed   (EShell *shell,
				   const char *uri,
				   const char *type_name,
				   EShellView *shell_view)
{
	EShellPrivate *priv;
	GtkWindow *parent_window;
	GList *p;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (type_name != NULL);
	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	priv = shell->priv;

	/* See if that type has caused a crash already.  */

	for (p = priv->crash_type_names; p != NULL; p = p->next) {
		const char *crash_type_name;

		crash_type_name = (const char *) p->data;
		if (strcmp (type_name, crash_type_name) == 0) {
			/* This type caused a crash already.  */
			return;
		}
	}

	/* New crash.  */

	priv->crash_type_names = g_list_prepend (priv->crash_type_names, g_strdup (type_name));

	if (shell_view == NULL)
		parent_window = NULL;
	else
		parent_window = GTK_WINDOW (shell_view);

	e_notice (parent_window, GNOME_MESSAGE_BOX_ERROR,
		  _("Ooops!  The view for `%s' have died unexpectedly.  :-(\n"
		    "This probably means that the %s component has crashed."),
		  uri, type_name);

	if (shell_view)
		bonobo_window_deregister_dead_components (BONOBO_WINDOW (shell_view));

	/* FIXME: we should probably re-start the component here */
}


E_MAKE_TYPE (e_shell, "EShell", EShell, class_init, init, PARENT_TYPE)
