/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#elif defined (GDK_WINDOWING_WIN32)
/* gdkwin32.h includes <windows.h> which stomps over the namespace */
#undef DATADIR
#define interface windows_interface
#include <gdk/gdkwin32.h>
#undef interface
#endif

#include <glib/gi18n.h>

#include <gconf/gconf-client.h>

#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>

#include <libedataserver/e-xml-utils.h>
#include <libedataserverui/e-passwords.h>

#include "e-util/e-bconf-map.h"
#include "e-util/e-dialog-utils.h"
#include "e-util/e-error.h"
#include "e-util/e-fsutils.h"
#include "e-util/e-util.h"

#include "Evolution.h"
#include "e-shell-constants.h"
#include "e-shell-settings-dialog.h"
#include "e-shell.h"
#include "e-shell-view.h"
#include "es-event.h"
#include "evolution-listener.h"
#include "evolution-shell-component-utils.h"

static void set_line_status_complete(EvolutionListener *el, gpointer data);

#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;
static gboolean session_started = FALSE;

struct _EShellPrivate {
	/* IID for registering the object on OAF.  */
	gchar *iid;

	GList *windows;

	/* EUriSchemaRegistry *uri_schema_registry; FIXME */
	EComponentRegistry *component_registry;

	/* Names for the types of the folders that have maybe crashed.  */
	/* FIXME TODO */
	GList *crash_type_names; /* gchar * */

	/* Line status and controllers  */
	EShellLineStatus line_status;
	gint line_status_pending;
	EShellLineStatus line_status_working;
	EvolutionListener *line_status_listener;

	/* Settings Dialog */
	union {
		GtkWidget *widget;
		gpointer pointer;
	} settings_dialog;

	/* If we're quitting and things are still busy, a timeout handler */
	guint quit_timeout;

	/* Whether the shell is succesfully initialized.  This is needed during
	   the start-up sequence, to avoid CORBA calls to do make wrong things
	   to happen while the shell is initializing.  */
	guint is_initialized : 1;

	/* Wether the shell is working in "interactive" mode or not.
	   (Currently, it's interactive IIF there is at least one active
	   view.)  */
	guint is_interactive : 1;

	/* Whether quit has been requested, and the shell is now waiting for
	   permissions from all the components to quit.  */
	guint preparing_to_quit : 1;

	/* Whether we are recovering from a crash in the previous session. */
	guint crash_recovery : 1;
};

/* Signals.  */

enum {
	NO_WINDOWS_LEFT,
	LINE_STATUS_CHANGED,
	NEW_WINDOW_CREATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* Utility functions.  */

static gboolean
get_config_start_offline (void)
{
	GConfClient *client;
	gboolean value;

	client = gconf_client_get_default ();

	value = gconf_client_get_bool (client, "/apps/evolution/shell/start_offline", NULL);

	g_object_unref (client);

	return value;
}

/* Interactivity handling.  */

static void
set_interactive (EShell *shell,
		 gboolean interactive)
{
	GSList *component_list;
	GSList *p;
	GList *first_element;
	gint num_windows;
	GtkWidget *view;

	g_return_if_fail (E_IS_SHELL (shell));

	shell->priv->is_interactive = interactive;

	num_windows = g_list_length (shell->priv->windows);

	/* We want to send the "interactive" message only when the first
	window is created */
	if (num_windows != 1)
		return;

	first_element = g_list_first (shell->priv->windows);
	view = GTK_WIDGET (first_element->data);

	component_list = e_component_registry_peek_list (shell->priv->component_registry);

	for (p = component_list; p != NULL; p = p->next) {
		EComponentInfo *info = p->data;
		CORBA_Environment ev;

		CORBA_exception_init (&ev);

#ifdef GDK_WINDOWING_X11
		GNOME_Evolution_Component_interactive (info->iface, interactive,GPOINTER_TO_INT (GDK_WINDOW_XWINDOW (view->window)), &ev);
#elif defined (GDK_WINDOWING_WIN32)
		GNOME_Evolution_Component_interactive (info->iface, interactive,GPOINTER_TO_INT (GDK_WINDOW_HWND (view->window)), &ev);
#else
#error Port this to your windowing system
#endif

		/* Ignore errors, the components can decide to not implement
		   this interface. */

		CORBA_exception_free (&ev);
	}
}

/* CORBA interface implementation.  */

static gboolean
raise_exception_if_not_ready (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	EShell *shell;

	shell = E_SHELL (bonobo_object_from_servant (servant));

	if (! shell->priv->is_initialized) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Shell_NotReady, NULL);
		return TRUE;
	}

	return FALSE;
}

static GNOME_Evolution_ShellView
impl_Shell_createNewWindow (PortableServer_Servant servant,
			    const CORBA_char *component_id,
			    CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EShell *shell;
	EShellWindow *shell_window;
	EShellView *shell_view;

	if (raise_exception_if_not_ready (servant, ev))
		return CORBA_OBJECT_NIL;

	bonobo_object = bonobo_object_from_servant (servant);
	shell = E_SHELL (bonobo_object);

	if (component_id[0] == '\0')
		component_id = NULL;

	shell_window = e_shell_create_window (shell, component_id, NULL);
	if (shell_window == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Shell_ComponentNotFound, NULL);
		return CORBA_OBJECT_NIL;
	}

	/* refs?? */
	shell_view = e_shell_view_new(shell_window);

	return BONOBO_OBJREF(shell_view);

}

static void
impl_Shell_handleURI (PortableServer_Servant servant,
		      const CORBA_char *uri,
		      CORBA_Environment *ev)
{
	EShell *shell = E_SHELL (bonobo_object_from_servant (servant));
	EComponentInfo *component_info;
	gchar *schema, *p;
	gint show = FALSE;

	schema = g_alloca(strlen(uri)+1);
	strcpy(schema, uri);
	p = strchr(schema, ':');
	if (p)
		*p = 0;

	component_info = e_component_registry_peek_info(shell->priv->component_registry, ECR_FIELD_SCHEMA, schema);
	if (component_info == NULL) {
		show = TRUE;
		component_info = e_component_registry_peek_info(shell->priv->component_registry, ECR_FIELD_ALIAS, schema);
	}

	if (component_info == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Shell_UnsupportedSchema, NULL);
		return;
	}

	if (show) {
		GtkWidget *shell_window;

		shell_window = (GtkWidget *)e_shell_create_window (shell, component_info->id, NULL);
		if (shell_window == NULL) {
			CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Shell_ComponentNotFound, NULL);
			return;
		}
	}

	GNOME_Evolution_Component_handleURI (component_info->iface, uri, ev);
	/* not an error not to implement it */
	if (ev->_id != NULL && strcmp(ev->_id, ex_CORBA_NO_IMPLEMENT) == 0)
		memset(ev, 0, sizeof(*ev));
}

static void
impl_Shell_setLineStatus (PortableServer_Servant servant,
			  CORBA_boolean online,
			  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EShell *shell;

	if (raise_exception_if_not_ready (servant, ev))
		return;

	bonobo_object = bonobo_object_from_servant (servant);
	shell = E_SHELL (bonobo_object);

	/* let the password manager know out online status */
	e_passwords_set_online(online);

	if (online)
		e_shell_set_line_status (shell, GNOME_Evolution_USER_ONLINE);
	else
		e_shell_set_line_status (shell, GNOME_Evolution_USER_OFFLINE);
}
/*
static GNOME_Evolution_Component
impl_Shell_findComponent(PortableServer_Servant servant,
			 const CORBA_char *id,
			 CORBA_Environment *ev)
{
	EShell *shell;
	EComponentInfo *ci;

	if (raise_exception_if_not_ready (servant, ev))
		return CORBA_OBJECT_NIL;

	shell = (EShell *)bonobo_object_from_servant (servant);
	ci = e_component_registry_peek_info(shell->priv->component_registry, ECR_FIELD_ALIAS, id);
	if (ci == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Shell_ComponentNotFound, NULL);
		return CORBA_OBJECT_NIL;
	} else if (ci->iface == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Shell_NotReady, NULL);
		return CORBA_OBJECT_NIL;
	} else {
		return ci->iface;
	}
}
*/

/* EShellWindow handling and bookkeeping.  */

static gint
window_delete_event_cb (GtkWidget *widget,
		      GdkEventAny *ev,
		      gpointer data)
{
	EShell *shell;

	g_return_val_if_fail (E_IS_SHELL_WINDOW (widget), TRUE);
	shell = E_SHELL (data);

	return ! e_shell_request_close_window (shell, E_SHELL_WINDOW (widget));
}

static gboolean
notify_no_windows_left_idle_cb (gpointer data)
{
	EShell *shell;
	EShellPrivate *priv;

	shell = E_SHELL (data);
	priv = shell->priv;

	set_interactive (shell, FALSE);

	g_signal_emit (shell, signals [NO_WINDOWS_LEFT], 0);

	if (priv->iid != NULL)
		bonobo_activation_active_server_unregister (priv->iid,
							    bonobo_object_corba_objref (BONOBO_OBJECT (shell)));
	bonobo_object_unref (BONOBO_OBJECT (shell));

	return FALSE;
}

static void
window_weak_notify (gpointer data,
		    GObject *where_the_object_was)
{
	EShell *shell;
	gint num_windows;

	shell = E_SHELL (data);

	num_windows = g_list_length (shell->priv->windows);

	/* If this is our last window, save settings now because in the callback
	   for no_windows_left shell->priv->windows will be NULL and settings won't
	   be saved because of that.  */
	if (num_windows == 1)
		e_shell_save_settings (shell);

	shell->priv->windows = g_list_remove (shell->priv->windows, where_the_object_was);

	if (shell->priv->windows == NULL) {
		bonobo_object_ref (BONOBO_OBJECT (shell));
		g_idle_add (notify_no_windows_left_idle_cb, shell);
	}
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EShell *shell;
	EShellPrivate *priv;
	GList *p;

	shell = E_SHELL (object);
	priv = shell->priv;

	priv->is_initialized = FALSE;

#if 0				/* FIXME */
	if (priv->uri_schema_registry != NULL) {
		g_object_unref (priv->uri_schema_registry);
		priv->uri_schema_registry = NULL;
	}
#endif

	if (priv->component_registry != NULL) {
		g_object_unref (priv->component_registry);
		priv->component_registry = NULL;
	}

	if (priv->quit_timeout) {
		g_source_remove(priv->quit_timeout);
		priv->quit_timeout = 0;
	}

	for (p = priv->windows; p != NULL; p = p->next) {
		EShellWindow *window;

		window = E_SHELL_WINDOW (p->data);

		g_signal_handlers_disconnect_by_func (window, G_CALLBACK (window_delete_event_cb), shell);
		g_object_weak_unref (G_OBJECT (window), window_weak_notify, shell);

		gtk_object_destroy (GTK_OBJECT (window));
	}

	g_list_free (priv->windows);
	priv->windows = NULL;

	/* No unreffing for these as they are aggregate.  */
	/* bonobo_object_unref (BONOBO_OBJECT (priv->corba_storage_registry)); */

	if (priv->settings_dialog.widget != NULL) {
		gtk_widget_destroy (priv->settings_dialog.widget);
		priv->settings_dialog.widget = NULL;
	}

	if (priv->line_status_listener) {
		priv->line_status_listener->complete = NULL;
		bonobo_object_unref(BONOBO_OBJECT(priv->line_status_listener));
		priv->line_status_listener = NULL;
	}

	g_free (priv->iid);
	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EShell *shell;
	EShellPrivate *priv;

	shell = E_SHELL (object);
	priv = shell->priv;

	g_list_foreach (priv->crash_type_names, (GFunc) g_free, NULL);
	g_list_free (priv->crash_type_names);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Initialization.  */

static void
e_shell_class_init (EShellClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_Shell__epv *epv;

	parent_class = g_type_class_ref(PARENT_TYPE);

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	signals[NO_WINDOWS_LEFT] =
		g_signal_new ("no_windows_left",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EShellClass, no_windows_left),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[LINE_STATUS_CHANGED] =
		g_signal_new ("line_status_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EShellClass, line_status_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	signals[NEW_WINDOW_CREATED] =
		g_signal_new ("new_window_created",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EShellClass, new_window_created),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	epv = & klass->epv;
	epv->createNewWindow = impl_Shell_createNewWindow;
	epv->handleURI       = impl_Shell_handleURI;
	epv->setLineStatus   = impl_Shell_setLineStatus;
/*	epv->findComponent   = impl_Shell_findComponent;*/
}

static void
e_shell_init (EShell *shell)
{
	EShellPrivate *priv;

	priv = g_new0 (EShellPrivate, 1);
	priv->line_status                  = E_SHELL_LINE_STATUS_OFFLINE;
	priv->component_registry           = e_component_registry_new ();

	shell->priv = priv;

	priv->line_status_listener = evolution_listener_new(set_line_status_complete, shell);
}

static void
detect_version (GConfClient *gconf, gint *major, gint *minor, gint *revision)
{
	gchar *val, *evolution_dir;
	struct stat st;

	evolution_dir = g_build_filename (g_get_home_dir (), "evolution", NULL);

	val = gconf_client_get_string(gconf, "/apps/evolution/version", NULL);
	if (val) {
		/* Since 1.4.0 We've been keeping the version key in gconf */
		sscanf(val, "%d.%d.%d", major, minor, revision);
		g_free(val);
	} else if (g_lstat (evolution_dir, &st) != 0 || !S_ISDIR (st.st_mode)) {
		/* If ~/evolution does not exit or is not a directory it must be a new installation */
		*major = 0;
		*minor = 0;
		*revision = 0;
	} else {
		xmlDocPtr config_doc;
		xmlNodePtr source;
		gchar *tmp;

		tmp = g_build_filename (evolution_dir, "config.xmldb", NULL);
		config_doc = e_xml_parse_file (tmp);
		g_free (tmp);
		tmp = NULL;

		if (config_doc
		    && (source = e_bconf_get_path (config_doc, "/Shell"))
		    && (tmp = e_bconf_get_value (source, "upgrade_from_1_0_to_1_2_performed"))
		    && tmp[0] == '1' ) {
			*major = 1;
			*minor = 2;
			*revision = 0;
		} else {
			*major = 1;
			*minor = 0;
			*revision = 0;
		}
		g_free (tmp);
		if (config_doc)
			xmlFreeDoc (config_doc);
	}

	g_free (evolution_dir);
}

/* calls components to perform upgrade */
static gboolean
attempt_upgrade (EShell *shell, gint major, gint minor, gint revision)
{
	GSList *component_infos, *p;
	gboolean success;
	gint res;

	success = TRUE;

	component_infos = e_component_registry_peek_list (shell->priv->component_registry);
	for (p = component_infos; success && p != NULL; p = p->next) {
		const EComponentInfo *info = p->data;
		CORBA_Environment ev;

		CORBA_exception_init (&ev);

		GNOME_Evolution_Component_upgradeFromVersion (info->iface, major, minor, revision, &ev);

		if (BONOBO_EX (&ev)) {
			gchar *exception_text;
			CORBA_char *id = CORBA_exception_id(&ev);

			if (strcmp (id, ex_CORBA_NO_IMPLEMENT) == 0) {
				/* Ignore components that do not implement this version, it
				   might just mean that they don't need an upgrade path. */
			} else if (strcmp (id,  ex_GNOME_Evolution_Component_UpgradeFailed) == 0) {
				GNOME_Evolution_Component_UpgradeFailed *ex = CORBA_exception_value(&ev);

				res = e_error_run(NULL, "shell:upgrade-failed", ex->what, ex->why, NULL);
				if (res == GTK_RESPONSE_CANCEL)
					success = FALSE;
			} else if (strcmp (id,  ex_GNOME_Evolution_Component_UnsupportedVersion) == 0) {
				/* This is non-fatal */
				/* DO WE CARE??? */
				printf("Upgrade of component failed, unsupported prior version\n");
			} else {
				exception_text = bonobo_exception_get_text (&ev);
				res = e_error_run(NULL, "shell:upgrade-failed", exception_text, _("Unknown system error."), NULL);
				g_free (exception_text);
				if (res == GTK_RESPONSE_CANCEL)
					success = FALSE;
			}
		}
		CORBA_exception_free (&ev);
	}

	return success;
}

/**
 * e_shell_construct:
 * @shell: An EShell object to construct
 * @iid: OAFIID for registering the shell into the name server
 * @startup_line_mode: How to set up the line mode (online or offline) initally.
 *
 * Construct @shell so that it uses the specified @corba_object.
 *
 * Return value: The result of the operation.
 **/
EShellConstructResult
e_shell_construct (EShell *shell,
		   const gchar *iid,
		   EShellStartupLineMode startup_line_mode)
{
	EShellPrivate *priv;
	CORBA_Object corba_object;
	gboolean start_online;
	GSList *component;

	g_return_val_if_fail (E_IS_SHELL (shell), E_SHELL_CONSTRUCT_RESULT_INVALIDARG);
	g_return_val_if_fail (startup_line_mode == E_SHELL_STARTUP_LINE_MODE_CONFIG
			      || startup_line_mode == E_SHELL_STARTUP_LINE_MODE_ONLINE
			      || startup_line_mode == E_SHELL_STARTUP_LINE_MODE_OFFLINE,
			      E_SHELL_CONSTRUCT_RESULT_INVALIDARG);

	priv = shell->priv;
	priv->iid = g_strdup (iid);

	/* Now we can register into OAF.  Notice that we shouldn't be
	   registering into OAF until we are sure we can complete.  */

	/* FIXME: Multi-display stuff.  */
	corba_object = bonobo_object_corba_objref (BONOBO_OBJECT (shell));
	if (bonobo_activation_active_server_register (iid, corba_object) != Bonobo_ACTIVATION_REG_SUCCESS)
		return E_SHELL_CONSTRUCT_RESULT_CANNOTREGISTER;

	while (gtk_events_pending ())
		gtk_main_iteration ();

	/* activate all the components (peek list does this implictly) */
	/* Do we really need to assign the result of this to the list? */
	component = e_component_registry_peek_list (shell->priv->component_registry);

	e_shell_attempt_upgrade(shell);

	priv->is_initialized = TRUE;

	switch (startup_line_mode) {
	case E_SHELL_STARTUP_LINE_MODE_CONFIG:
		start_online = ! get_config_start_offline ();
		break;
	case E_SHELL_STARTUP_LINE_MODE_ONLINE:
		start_online = TRUE;
		break;
	case E_SHELL_STARTUP_LINE_MODE_OFFLINE:
		start_online = FALSE;
		break;
	default:
		start_online = FALSE; /* Make compiler happy.  */
		g_return_val_if_reached(E_SHELL_CONSTRUCT_RESULT_OK);
	}

	e_passwords_set_online(start_online);

	if (start_online)
		e_shell_set_line_status (shell, GNOME_Evolution_USER_ONLINE);
	else
		e_shell_set_line_status (shell, GNOME_Evolution_USER_OFFLINE);

	return E_SHELL_CONSTRUCT_RESULT_OK;
}

/**
 * e_shell_new:
 * @start_online: Whether to start in on-line mode or not.
 * @construct_result_return: A pointer to an EShellConstructResult variable into
 * which the result of the operation will be stored.
 *
 * Create a new EShell.
 *
 * Return value:
 **/
EShell *
e_shell_new (EShellStartupLineMode startup_line_mode,
	     EShellConstructResult *construct_result_return)
{
	EShell *new;
	EShellConstructResult construct_result;

	new = g_object_new (e_shell_get_type (), NULL);

	construct_result = e_shell_construct (new, E_SHELL_OAFIID, startup_line_mode);

	if (construct_result != E_SHELL_CONSTRUCT_RESULT_OK) {
		*construct_result_return = construct_result;
		bonobo_object_unref (BONOBO_OBJECT (new));
		return NULL;
	}

	*construct_result_return = E_SHELL_CONSTRUCT_RESULT_OK;
	return new;
}

static gint
remove_dir(const gchar *root, const gchar *path)
{
	GDir *dir;
	const gchar *dname;
	gint res = -1;
	gchar *new = NULL;
	struct stat st;

	dir = g_dir_open(path, 0, NULL);
	if (dir == NULL)
		return -1;

	while ( (dname = g_dir_read_name(dir)) ) {
		new = g_build_filename(path, dname, NULL);
		if (g_stat(new, &st) == -1)
			goto fail;

		/* make sure we're really removing something from evolution dir */
		g_return_val_if_fail (strlen(path) >= strlen(root)
			 && strncmp(root, path, strlen(root)) == 0, -1);

		if (S_ISDIR(st.st_mode)) {
			if (remove_dir(root, new) == -1)
				goto fail;
		} else {
			if (g_unlink(new) == -1)
				goto fail;
		}
		g_free(new);
		new = NULL;
	}

	res = g_rmdir(path);
fail:
	g_free(new);
	g_dir_close(dir);
	return res;
}

/**
 * e_shell_attempt_upgrade:
 * @shell:
 *
 * Upgrade config and components from the currently installed version.
 *
 * Return value: %TRUE If it works.  If it fails the application will exit.
 **/
gboolean
e_shell_attempt_upgrade (EShell *shell)
{
	GConfClient *gconf_client;
	gint major = 0, minor = 0, revision = 0;
	gint lmajor, lminor, lrevision;
	gint cmajor, cminor, crevision;
	gchar *version_string, *last_version = NULL;
	gint done_upgrade = FALSE;
	gchar *oldpath;
	struct stat st;
	ESEvent *ese;

	gconf_client = gconf_client_get_default();

	oldpath = g_build_filename(g_get_home_dir(), "evolution", NULL);

	g_return_val_if_fail (sscanf(BASE_VERSION, "%u.%u", &cmajor, &cminor) == 2, TRUE);
	crevision = atoi(UPGRADE_REVISION);

	detect_version (gconf_client, &major, &minor, &revision);

	if (!(cmajor > major
	      || (cmajor == major && cminor > minor)
	      || (cminor == minor && crevision > revision)))
		goto check_old;

	/* if upgrading from < 1.5, we need to copy most data from ~/evolution to ~/.evolution */
	if (major == 1 && minor < 5) {
		long size, space;

		size = e_fsutils_usage(oldpath);
		space = e_fsutils_avail(g_get_home_dir());
		if (size != -1 && space != -1 && space < size) {
			gchar *required = g_strdup_printf(_("%ld KB"), size);
			gchar *have = g_strdup_printf(_("%ld KB"), space);

			e_error_run(NULL, "shell:upgrade-nospace", required, have, NULL);
			g_free(required);
			g_free(have);
			_exit(0);
		}
	}

	if (!attempt_upgrade (shell, major, minor, revision))
		_exit(0);

	/* mark as upgraded */
	version_string = g_strdup_printf ("%s.%s", BASE_VERSION, UPGRADE_REVISION);
	gconf_client_set_string (gconf_client, "/apps/evolution/version", version_string, NULL);
	done_upgrade = TRUE;

check_old:
	/* if the last upgraded version was old, check for stuff to remove */
	if (done_upgrade
	    ||	(last_version = gconf_client_get_string (gconf_client, "/apps/evolution/last_version", NULL)) == NULL
	    ||  sscanf(last_version, "%d.%d.%d", &lmajor, &lminor, &lrevision) != 3) {
		lmajor = major;
		lminor = minor;
		lrevision = revision;
	}
	g_free(last_version);

	if (lmajor == 1 && lminor < 5
	    && g_stat(oldpath, &st) == 0
	    && S_ISDIR(st.st_mode)) {
		gint res;

		last_version = g_strdup_printf("%d.%d.%d", lmajor, lminor, lrevision);
		res = e_error_run(NULL, "shell:upgrade-remove-1-4", last_version, NULL);
		g_free(last_version);

		switch (res) {
		case GTK_RESPONSE_OK: /* 'delete' */
			if (e_error_run(NULL, "shell:upgrade-remove-1-4-confirm", NULL) == GTK_RESPONSE_OK)
				remove_dir(oldpath, oldpath);
			else
				break;
			/* falls through */
		case GTK_RESPONSE_ACCEPT: /* 'keep' */
			lmajor = cmajor;
			lminor = cminor;
			lrevision = crevision;
			break;
		default:
			/* cancel - noop */
			break;
		}
	} else {
		/* otherwise 'last version' is now the same as current */
		lmajor = cmajor;
		lminor = cminor;
		lrevision = crevision;
	}

	last_version = g_strdup_printf("%d.%d.%d", lmajor, lminor, lrevision);
	gconf_client_set_string (gconf_client, "/apps/evolution/last_version", last_version, NULL);
	g_free(last_version);

	g_free(oldpath);
	g_object_unref (gconf_client);

	/** @Event: Shell attempted upgrade
	 * @Id: upgrade.done
	 * @Target: ESMenuTargetState
	 *
	 * This event is emitted whenever the shell successfully attempts an upgrade.
	 *
	 */
	ese = es_event_peek();
	e_event_emit((EEvent *)ese, "upgrade.done", (EEventTarget *)es_event_target_new_upgrade(ese, cmajor, cminor, crevision));

	return TRUE;
}

/**
 * e_shell_create_window:
 * @shell: The shell for which to create a new window.
 * @component_id: Id or alias of the component to display in the new window.
 * @template_window: Window from which to copy the window settings (can be %NULL).
 *
 * Create a new window for @uri.
 *
 * Return value: The new window.
 **/
EShellWindow *
e_shell_create_window (EShell *shell,
		       const gchar *component_id,
		       EShellWindow *template_window)
{
	EShellWindow *window;

	/* FIXME need to actually copy settings from template_window.  */

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	window = E_SHELL_WINDOW (e_shell_window_new (shell, component_id));

	g_signal_connect (window, "delete_event", G_CALLBACK (window_delete_event_cb), shell);
	g_object_weak_ref (G_OBJECT (window), window_weak_notify, shell);
	shell->priv->windows = g_list_prepend (shell->priv->windows, window);

	g_signal_emit (shell, signals[NEW_WINDOW_CREATED], 0, window);

	gtk_widget_show (GTK_WIDGET (window));

	e_error_default_parent((GtkWindow *)window);

	set_interactive (shell, TRUE);

	if (!session_started) {
		ESEvent *ese;

		session_started = TRUE;
		ese = es_event_peek();
		e_event_emit((EEvent *)ese, "started.done", (EEventTarget *)es_event_target_new_shell(ese, shell));
	}
	return window;
}

gboolean
e_shell_request_close_window (EShell *shell,
			      EShellWindow *shell_window)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);
	g_return_val_if_fail (E_IS_SHELL_WINDOW (shell_window), FALSE);

	e_shell_save_settings (shell);

	if (g_list_length (shell->priv->windows) != 1) {
		/* Not the last window.  */
		return TRUE;
	}

	return e_shell_quit(shell);
}

#if 0				/* FIXME */
/**
 * e_shell_peek_uri_schema_registry:
 * @shell: An EShell object.
 *
 * Get the schema registry associated to @shell.
 *
 * Return value: A pointer to the EUriSchemaRegistry associated to @shell.
 **/
EUriSchemaRegistry  *
e_shell_peek_uri_schema_registry (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->uri_schema_registry;
}
#endif

/**
 * e_shell_peek_component_registry:
 * @shell:
 *
 * Get the component registry associated to @shell.
 *
 * Return value:
 **/
EComponentRegistry *
e_shell_peek_component_registry (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->component_registry;
}

/**
 * e_shell_save_settings:
 * @shell:
 *
 * Save the settings for this shell.
 *
 * Return value: %TRUE if it worked, %FALSE otherwise.  Even if %FALSE is
 * returned, it is possible that at least part of the settings for the windows
 * have been saved.
 **/
gboolean
e_shell_save_settings (EShell *shell)
{
	GConfClient *client;
	gboolean is_offline;

	is_offline = ( e_shell_get_line_status (shell) == E_SHELL_LINE_STATUS_OFFLINE );

	client = gconf_client_get_default ();
	gconf_client_set_bool (client, "/apps/evolution/shell/start_offline", is_offline, NULL);
	g_object_unref (client);

	return TRUE;
}

/**
 * e_shell_close_all_windows:
 * @shell:
 *
 * Destroy all the windows in @shell.
 **/
void
e_shell_close_all_windows (EShell *shell)
{
	EShellPrivate *priv;
	GList *p, *pnext;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	if (shell->priv->windows)
		e_shell_save_settings (shell);

	priv = shell->priv;
	for (p = priv->windows; p != NULL; p = pnext) {
		pnext = p->next;

		/* Note that this will also remove the window from the list... Hence the
		   need for the pnext variable.  */
		gtk_widget_destroy (GTK_WIDGET (p->data));
	}
}

/**
 * e_shell_get_line_status:
 * @shell: A pointer to an EShell object.
 *
 * Get the line status for @shell.
 *
 * Return value: The current line status for @shell.
 **/
EShellLineStatus
e_shell_get_line_status (EShell *shell)
{
	g_return_val_if_fail (shell != NULL, E_SHELL_LINE_STATUS_OFFLINE);
	g_return_val_if_fail (E_IS_SHELL (shell), E_SHELL_LINE_STATUS_OFFLINE);

	return shell->priv->line_status;
}

/* Offline/online handling.  */

static void
set_line_status_finished(EShell *shell)
{
	EShellPrivate *priv = shell->priv;
	ESEvent *ese;

	priv->line_status = priv->line_status_working;

	e_passwords_set_online (priv->line_status == E_SHELL_LINE_STATUS_ONLINE);
	g_signal_emit (shell, signals[LINE_STATUS_CHANGED], 0, priv->line_status);

	/** @Event: Shell online state changed
	 * @Id: state.changed
	 * @Target: ESMenuTargetState
	 *
	 * This event is emitted whenever the shell online state changes.
	 *
	 * Only the online and offline states are emitted.
	 */
	ese = es_event_peek();
	e_event_emit((EEvent *)ese, "state.changed", (EEventTarget *)es_event_target_new_state(ese, priv->line_status == E_SHELL_LINE_STATUS_ONLINE));
}

static void
set_line_status_complete(EvolutionListener *el, gpointer data)
{
	EShell *shell = data;
	EShellPrivate *priv = shell->priv;

	if (priv->line_status_pending > 0) {
		priv->line_status_pending--;
		if (priv->line_status_pending == 0)
			set_line_status_finished(shell);
	}
}

void
e_shell_set_line_status (EShell *shell,
                         GNOME_Evolution_ShellState shell_state)
{
	EShellPrivate *priv;
	GSList *component_infos;
	GSList *p;
	CORBA_Environment ev;
	GConfClient *client;
	gboolean is_online;
	gboolean forced = FALSE;

	priv = shell->priv;

	if (shell_state == GNOME_Evolution_FORCED_OFFLINE || shell_state == GNOME_Evolution_USER_OFFLINE) {
		is_online = FALSE;
		if (shell_state == GNOME_Evolution_FORCED_OFFLINE)
			forced = TRUE;
	} else
		is_online = TRUE;

	if ((is_online && priv->line_status == E_SHELL_LINE_STATUS_ONLINE)
	    || (!is_online && priv->line_status == E_SHELL_LINE_STATUS_OFFLINE && !forced))
		return;

	/* we use 'going offline' to mean 'changing status' now */
	priv->line_status = E_SHELL_LINE_STATUS_GOING_OFFLINE;
	g_signal_emit (shell, signals[LINE_STATUS_CHANGED], 0, priv->line_status);

	client = gconf_client_get_default ();
	if (!forced)
		gconf_client_set_bool (client, "/apps/evolution/shell/start_offline", !is_online, NULL);
	g_object_unref (client);

	priv->line_status_working = is_online ? E_SHELL_LINE_STATUS_ONLINE: forced?E_SHELL_LINE_STATUS_FORCED_OFFLINE:E_SHELL_LINE_STATUS_OFFLINE;
	/* we start at 2: setLineStatus could recursively call back, we therefore
	   `need to not complete till we're really complete */
	priv->line_status_pending += 2;

	component_infos = e_component_registry_peek_list (priv->component_registry);
	for (p = component_infos; p != NULL; p = p->next) {
		EComponentInfo *info = p->data;

		CORBA_exception_init (&ev);

		GNOME_Evolution_Component_setLineStatus(info->iface, shell_state, bonobo_object_corba_objref((BonoboObject *)priv->line_status_listener), &ev);
		if (ev._major == CORBA_NO_EXCEPTION)
			priv->line_status_pending++;

		CORBA_exception_free (&ev);
	}

	priv->line_status_pending -= 2;
	if (priv->line_status_pending == 0)
		set_line_status_finished(shell);
}

gboolean
e_shell_get_crash_recovery (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->crash_recovery;
}

void
e_shell_set_crash_recovery (EShell *shell,
                            gboolean crash_recovery)
{
	g_return_if_fail (E_IS_SHELL (shell));

	shell->priv->crash_recovery = crash_recovery;
}

void
e_shell_send_receive (EShell *shell)
{
	GSList *component_list;
	GSList *p;

	g_return_if_fail (E_IS_SHELL (shell));

	component_list = e_component_registry_peek_list (shell->priv->component_registry);

	for (p = component_list; p != NULL; p = p->next) {
		EComponentInfo *info = p->data;
		CORBA_Environment ev;

		CORBA_exception_init (&ev);

		GNOME_Evolution_Component_sendAndReceive (info->iface, &ev);

		/* Ignore errors, the components can decide to not implement
		   this interface. */

		CORBA_exception_free (&ev);
	}
}

void
e_shell_show_settings (EShell *shell,
		       const gchar *type,
		       EShellWindow *shell_window)
{
	EShellPrivate *priv;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	priv = shell->priv;

	if (priv->settings_dialog.widget != NULL) {
		gdk_window_show (priv->settings_dialog.widget->window);
		gtk_widget_grab_focus (priv->settings_dialog.widget);
		return;
	}

	priv->settings_dialog.widget = e_shell_settings_dialog_new ();

	if (type != NULL)
		e_shell_settings_dialog_show_type (E_SHELL_SETTINGS_DIALOG (priv->settings_dialog.widget), type);

	g_object_add_weak_pointer (G_OBJECT (priv->settings_dialog.widget), &priv->settings_dialog.pointer);

	gtk_window_set_transient_for (GTK_WINDOW (priv->settings_dialog.widget), GTK_WINDOW (shell_window));
	gtk_widget_show (priv->settings_dialog.widget);
}

const gchar *
e_shell_construct_result_to_string (EShellConstructResult result)
{
	switch (result) {
	case E_SHELL_CONSTRUCT_RESULT_OK:
		return _("OK");
	case E_SHELL_CONSTRUCT_RESULT_INVALIDARG:
		return _("Invalid arguments");
	case E_SHELL_CONSTRUCT_RESULT_CANNOTREGISTER:
		return _("Cannot register on OAF");
	case E_SHELL_CONSTRUCT_RESULT_NOCONFIGDB:
		return _("Configuration Database not found");
	case E_SHELL_CONSTRUCT_RESULT_GENERICERROR:
		return _("Generic error");
	default:
		return _("Unknown error");
	}
}

/* timeout handler, so returns TRUE if we can't quit yet */
static gboolean
es_run_quit(EShell *shell)
{
	EShellPrivate *priv;
	GSList *component_infos;
	GSList *sp;
	CORBA_boolean done_quit;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	priv = shell->priv;
	priv->preparing_to_quit = TRUE;

	component_infos = e_component_registry_peek_list (priv->component_registry);
	done_quit = TRUE;
	for (sp = component_infos; sp != NULL; sp = sp->next) {
		EComponentInfo *info = sp->data;
		CORBA_Environment ev;

		CORBA_exception_init (&ev);

		done_quit = GNOME_Evolution_Component_quit(info->iface, &ev);
		if (BONOBO_EX (&ev)) {
			/* The component might not implement the interface, in which case we assume we can quit. */
			done_quit = TRUE;
		}

		CORBA_exception_free (&ev);

		if (!done_quit)
			break;
	}

	if (done_quit) {
		if  (priv->quit_timeout) {
			g_source_remove(priv->quit_timeout);
			priv->quit_timeout = 0;
		}
		e_shell_close_all_windows(shell);
	} else if (priv->quit_timeout == 0) {
		priv->quit_timeout = g_timeout_add(500, (GSourceFunc)es_run_quit, shell);
	}

	return !done_quit;
}

gboolean
e_shell_can_quit (EShell *shell)
{
	EShellPrivate *priv;
	GSList *component_infos;
	GSList *sp;
	CORBA_boolean can_quit;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	priv = shell->priv;

	if (priv->preparing_to_quit)
		return FALSE;

	component_infos = e_component_registry_peek_list (priv->component_registry);
	can_quit = TRUE;
	for (sp = component_infos; sp != NULL; sp = sp->next) {
		EComponentInfo *info = sp->data;
		CORBA_Environment ev;

		CORBA_exception_init (&ev);

		can_quit = GNOME_Evolution_Component_requestQuit (info->iface, &ev);
		if (BONOBO_EX (&ev)) {
			/* The component might not implement the interface, in which case we assume we can quit. */
			can_quit = TRUE;
		}

		CORBA_exception_free (&ev);

		if (! can_quit)
			break;
	}

	return can_quit;
}

gboolean
e_shell_do_quit (EShell *shell)
{
	EShellPrivate *priv;
	GList *p;
	gboolean can_quit;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	priv = shell->priv;

	if (priv->preparing_to_quit)
		return FALSE;

	for (p = shell->priv->windows; p != NULL; p = p->next) {
		gtk_widget_set_sensitive (GTK_WIDGET (p->data), FALSE);

		if (p == shell->priv->windows)
			e_shell_window_save_defaults (p->data);
	}

	can_quit = !es_run_quit (shell);

	/* Mark a safe quit by destroying the lock. */
	e_file_lock_destroy ();

	return can_quit;
}

gboolean
e_shell_quit (EShell *shell)
{
	return e_shell_can_quit (shell) && e_shell_do_quit (shell);
}

/**
 * gboolean (*EMainShellFunc) (EShell *shell, EShellWindow *window, gpointer user_data);
 * Function used in @ref e_shell_foreach_shell_window.
 * @param shell Pointer to EShell.
 * @param window Pointer to EShellWindow.
 * @param user_data User's data passed to @ref main_shell_foreach_shell_window.
 * @return TRUE if need to go to next window, FALSE when stop looking for next window.
 **/

/**
 * e_shell_foreach_shell_window
 * This will call function callback for all known EShellWindow of main shell.
 * When there is no shell active, then this will do nothing.
 * @param shell EShell instance.
 * @param func Function to be called.
 * @param user_data User data to pass to func.
 **/
void
e_shell_foreach_shell_window (EShell *shell, EMainShellFunc func, gpointer user_data)
{
	EShellPrivate *priv;
	GList *p;

	if (!shell)
		return;

	priv = shell->priv;

	for (p = priv->windows; p != NULL; p = p->next) {
		EShellWindow *window;

		window = E_SHELL_WINDOW (p->data);

		if (window && !func (shell, window, user_data))
			break;
	}
}

BONOBO_TYPE_FUNC_FULL (EShell, GNOME_Evolution_Shell, PARENT_TYPE, e_shell)
