/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* main.c
 *
 * Copyright (C) 2000, 2001, 2002, 2003  Ximian, Inc.
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

#include <config.h>

#include "e-util/e-dialog-utils.h"
#include "e-util/e-gtk-utils.h"

#include "e-icon-factory.h"
#include "e-shell-constants.h"
#include "e-shell-window.h"	/* FIXME */

#include "e-shell.h"

#include <gconf/gconf-client.h>

#include <gtk/gtkalignment.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>

#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-window-icon.h>

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>

#include <bonobo-activation/bonobo-activation.h>

#include <glade/glade.h>

#include "e-config-upgrade.h"
#include "Evolution-DataServer.h"

#ifdef GTKHTML_HAVE_GCONF
#include <gconf/gconf.h>
#endif

#include <gal/widgets/e-cursors.h>

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pthread.h>


#define DEVELOPMENT_WARNING


static EShell *shell = NULL;

/* Command-line options.  */
static gboolean start_online = FALSE;
static gboolean start_offline = FALSE;
static gboolean setup_only = FALSE;
static gboolean killev = FALSE;
static char *default_component_id = NULL;

static char *evolution_debug_log = NULL;


static GtkWidget *
quit_box_new (void)
{
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *frame;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);

	/* (Just to prevent smart-ass window managers like Sawfish from setting
	   the make the dialog as big as the standard Evolution window).  */
	gtk_window_set_wmclass (GTK_WINDOW (window), "evolution-quit", "Evolution:quit");

	e_make_widget_backing_stored (window);

	gtk_window_set_title (GTK_WINDOW (window), _("Evolution"));

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (window), frame);

	label = gtk_label_new (_("Evolution is now exiting ..."));
	gtk_misc_set_padding (GTK_MISC (label), 30, 25);

	gtk_container_add (GTK_CONTAINER (frame), label);

	gtk_widget_show_now (frame);
	gtk_widget_show_now (label);
	gtk_widget_show_now (window);

	/* For some reason, the window fails to update without this
	   sometimes.  */
	gtk_widget_queue_draw (window);
	gtk_widget_queue_draw (label);
	gtk_widget_queue_draw (frame);

	gdk_flush ();

	while (gtk_events_pending ())
		gtk_main_iteration ();

	gdk_flush ();

	return window;
}

static void
no_windows_left_cb (EShell *shell, gpointer data)
{
	GtkWidget *quit_box;

	quit_box = quit_box_new ();
	g_object_add_weak_pointer (G_OBJECT (quit_box), (void **) &quit_box);

	bonobo_object_unref (BONOBO_OBJECT (shell));

	if (quit_box != NULL)
		gtk_widget_destroy (quit_box);

	bonobo_main_quit ();
}

static void
shell_weak_notify (void *data,
		   GObject *where_the_object_was)
{
	bonobo_main_quit ();
}


#ifdef KILL_PROCESS_CMD

static void
kill_dataserver (void)
{
	g_print ("(Killing old version of evolution-data-server...)\n");

	system (KILL_PROCESS_CMD " -9 lt-evolution-data-server 2> /dev/null");
	system (KILL_PROCESS_CMD " -9 evolution-data-server 2> /dev/null");

	system (KILL_PROCESS_CMD " -9 lt-evolution-alarm-notify 2> /dev/null");
	system (KILL_PROCESS_CMD " -9 evolution-alarm-notify 2> /dev/null");
}

static void
kill_old_dataserver (void)
{
	GNOME_Evolution_DataServer_InterfaceCheck iface;
	CORBA_Environment ev;
	CORBA_char *version;

	CORBA_exception_init (&ev);

	iface = bonobo_activation_activate_from_id ("OAFIID:GNOME_Evolution_DataServer_InterfaceCheck", 0, NULL, &ev);
	if (BONOBO_EX (&ev) || iface == CORBA_OBJECT_NIL) {
		kill_dataserver ();
		CORBA_exception_free (&ev);
		return;
	}

	version = GNOME_Evolution_DataServer_InterfaceCheck__get_interfaceVersion (iface, &ev);
	if (BONOBO_EX (&ev)) {
		kill_dataserver ();
		CORBA_Object_release (iface, &ev);
		CORBA_exception_free (&ev);
		return;
	}

	if (strcmp (version, DATASERVER_VERSION) != 0) {
		CORBA_free (version);
		kill_dataserver ();
		CORBA_Object_release (iface, &ev);
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_free (version);
	CORBA_Object_release (iface, &ev);
	CORBA_exception_free (&ev);
}
#endif


#ifdef DEVELOPMENT_WARNING

/* Warning dialog to scare people off a little bit.  */

static void
warning_dialog_response_callback (GtkDialog *dialog,
				 int button_number,
				 void *data)
{
	GtkCheckButton *dont_bother_me_again_checkbox;
	GConfClient *client;

	dont_bother_me_again_checkbox = GTK_CHECK_BUTTON (data);

	client = gconf_client_get_default ();
	gconf_client_set_bool (client, "/apps/evolution/shell/skip_warning_dialog",
			       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dont_bother_me_again_checkbox)),
			       NULL);
	g_object_unref (client);

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
show_development_warning (GtkWindow *parent)
{
	GtkWidget *label;
	GtkWidget *warning_dialog;
	GtkWidget *dont_bother_me_again_checkbox;
	GtkWidget *alignment;
	GConfClient *client;
	char *text;

	client = gconf_client_get_default ();

	if (gconf_client_get_bool (client, "/apps/evolution/shell/skip_warning_dialog", NULL)) {
		g_object_unref (client);
		return;
	}

	g_object_unref (client);

	warning_dialog = gtk_dialog_new_with_buttons("Ximian Evolution " VERSION, parent,
						     GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	text = g_strdup_printf(
		/* xgettext:no-c-format */
		/* Preview/Alpha/Beta version warning message */
		_("Hi.  Thanks for taking the time to download this preview release\n"
		  "of the Ximian Evolution groupware suite.\n"
		  "\n"
		  "This version of Ximian Evolution is not yet complete. It is getting close,\n"
		  "but some features are either unfinished or do not work properly.\n"
		  "\n"
		  "If you want a stable version of Evolution, we urge you to uninstall\n"
		  "this version, and install version %s instead.\n"
		  "\n"
		  "If you find bugs, please report them to us at bugzilla.ximian.com.\n"
                  "This product comes with no warranty and is not intended for\n"
		  "individuals prone to violent fits of anger.\n"
                  "\n"
		  "We hope that you enjoy the results of our hard work, and we\n"
		  "eagerly await your contributions!\n"),
		"1.4");
	label = gtk_label_new (text);
	g_free(text);

	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (warning_dialog)->vbox), 
			    label, TRUE, TRUE, 4);

	label = gtk_label_new (_("Thanks\n"
				 "The Ximian Evolution Team\n"));
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(label), 1, .5);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (warning_dialog)->vbox), 
			    label, TRUE, TRUE, 0);

	dont_bother_me_again_checkbox = gtk_check_button_new_with_label (_("Don't tell me again"));

	/* GTK sucks.  (Just so you know.)  */
	alignment = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);

	gtk_container_add (GTK_CONTAINER (alignment), dont_bother_me_again_checkbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (warning_dialog)->vbox),
			    alignment, FALSE, FALSE, 0);

	gtk_widget_show_all (warning_dialog);

	g_signal_connect (warning_dialog, "response",
			  G_CALLBACK (warning_dialog_response_callback),
			  dont_bother_me_again_checkbox);
}

/* The following signal handlers are used to display the development warning as
   soon as the first view is created.  */

static void
window_map_callback (GtkWidget *widget,
		     void *data)
{
	g_signal_handlers_disconnect_by_func (widget, G_CALLBACK (window_map_callback), data);

	show_development_warning (GTK_WINDOW (widget));
}

static void
new_window_created_callback (EShell *shell,
			     EShellWindow *window,
			     void *data)
{
	g_signal_handlers_disconnect_by_func (shell, G_CALLBACK (new_window_created_callback), data);

	g_signal_connect (window, "map", G_CALLBACK (window_map_callback), NULL);
}

#endif /* DEVELOPMENT_WARNING */


static void
attempt_upgrade (EShell *shell)
{
	GConfClient *gconf_client = gconf_client_get_default ();
	char *previous_version = gconf_client_get_string (gconf_client, "/apps/evolution/version", NULL);

	if (previous_version != NULL) {
		if (! e_shell_attempt_upgrade (shell, previous_version))
			e_notice (NULL, GTK_MESSAGE_ERROR,
				  _("Warning: Evolution could not upgrade all your data from version %s.\n"
				    "The data hasn't been deleted, but it will not be seen by this version of Evolution.\n"),
				  previous_version);
	}

	gconf_client_set_string (gconf_client, "/apps/evolution/version", VERSION, NULL);
	g_object_unref (gconf_client);
}


/* This is for doing stuff that requires the GTK+ loop to be running already.  */

static gint
idle_cb (void *data)
{
	GSList *uri_list;
	GNOME_Evolution_Shell corba_shell;
	CORBA_Environment ev;
	EShellConstructResult result;
	EShellStartupLineMode startup_line_mode;
	GSList *p;
	gboolean have_evolution_uri;

#ifdef KILL_PROCESS_CMD
	kill_old_dataserver ();
#endif

	CORBA_exception_init (&ev);

	uri_list = (GSList *) data;

	if (! start_online && ! start_offline)
		startup_line_mode = E_SHELL_STARTUP_LINE_MODE_CONFIG;
	else if (start_online)
		startup_line_mode = E_SHELL_STARTUP_LINE_MODE_ONLINE;
	else
		startup_line_mode = E_SHELL_STARTUP_LINE_MODE_OFFLINE;

	shell = e_shell_new (startup_line_mode, &result);

	switch (result) {
	case E_SHELL_CONSTRUCT_RESULT_OK:
		g_signal_connect (shell, "no_windows_left", G_CALLBACK (no_windows_left_cb), NULL);
		g_object_weak_ref (G_OBJECT (shell), shell_weak_notify, NULL);

#ifdef DEVELOPMENT_WARNING
		if (!getenv ("EVOLVE_ME_HARDER"))
			g_signal_connect (shell, "new_window_created",
					  G_CALLBACK (new_window_created_callback), NULL);
#endif

		corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell));
		corba_shell = CORBA_Object_duplicate (corba_shell, &ev);
		break;

	case E_SHELL_CONSTRUCT_RESULT_CANNOTREGISTER:
		corba_shell = bonobo_activation_activate_from_id (E_SHELL_OAFIID, 0, NULL, &ev);
		if (ev._major != CORBA_NO_EXCEPTION || corba_shell == CORBA_OBJECT_NIL) {
			e_notice (NULL, GTK_MESSAGE_ERROR,
				  _("Cannot access the Ximian Evolution shell."));
			CORBA_exception_free (&ev);
			bonobo_main_quit ();
			return FALSE;
		}
		break;

	default:
		e_notice (NULL, GTK_MESSAGE_ERROR,
			  _("Cannot initialize the Ximian Evolution shell: %s"),
			  e_shell_construct_result_to_string (result));
		CORBA_exception_free (&ev);
		bonobo_main_quit ();
		return FALSE;

	}

	if (shell != NULL)
		attempt_upgrade (shell);

	have_evolution_uri = FALSE;

	if (shell != NULL) {
		e_shell_create_window (shell, default_component_id, NULL);
	} else {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		if (default_component_id == NULL)
			GNOME_Evolution_Shell_createNewWindow (corba_shell, "", &ev);
		else
			GNOME_Evolution_Shell_createNewWindow (corba_shell, default_component_id, &ev);
		CORBA_exception_free (&ev);
	}

	for (p = uri_list; p != NULL; p = p->next) {
		const char *uri;

		uri = (const char *) p->data;
		GNOME_Evolution_Shell_handleURI (corba_shell, uri, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("Invalid URI: %s", uri);
			CORBA_exception_free (&ev);
		}
	}

	g_slist_free (uri_list);

	CORBA_Object_release (corba_shell, &ev);

	CORBA_exception_free (&ev);
	
	if (shell == NULL)
		bonobo_main_quit ();

	return FALSE;
}


/* SIGSEGV handling.
   
   The GNOME SEGV handler will lose if it's not run from the main Gtk
   thread. So if we have to redirect the signal if the crash happens in another
   thread.  */

static void (*gnome_segv_handler) (int);
static GStaticMutex segv_mutex = G_STATIC_MUTEX_INIT;
static pthread_t main_thread;

static void
segv_redirect (int sig)
{
	if (pthread_self () == main_thread)
		gnome_segv_handler (sig);
	else {
		pthread_kill (main_thread, sig);

		/* We can't return from the signal handler or the thread may
		   SEGV again. But we can't pthread_exit, because then the
		   thread may get cleaned up before bug-buddy can get a stack
		   trace. So we block by trying to lock a mutex we know is
		   already locked.  */
		g_static_mutex_lock (&segv_mutex);
	}
}

static void
setup_segv_redirect (void)
{
	struct sigaction sa, osa;

	sigaction (SIGSEGV, NULL, &osa);
	if (osa.sa_handler == SIG_DFL)
		return;

	main_thread = pthread_self ();

	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);
	sa.sa_handler = segv_redirect;
	sigaction (SIGSEGV, &sa, NULL);
	sigaction (SIGBUS, &sa, NULL);
	sigaction (SIGFPE, &sa, NULL);
		
	sa.sa_handler = SIG_IGN;
	sigaction (SIGXFSZ, &sa, NULL);
	gnome_segv_handler = osa.sa_handler;
	g_static_mutex_lock (&segv_mutex);
}

int
main (int argc, char **argv)
{
	struct poptOption options[] = {
		{ "component", 'c', POPT_ARG_STRING, &default_component_id, 0,
		  N_("Start Evolution activating the specified component"), NULL },
		{ "offline", '\0', POPT_ARG_NONE, &start_offline, 0, 
		  N_("Start in offline mode"), NULL },
		{ "online", '\0', POPT_ARG_NONE, &start_online, 0, 
		  N_("Start in online mode"), NULL },
#ifdef KILL_PROCESS_CMD
		{ "force-shutdown", '\0', POPT_ARG_NONE, &killev, 0, 
		  N_("Forcibly shut down all evolution components"), NULL },
#endif
		{ "debug", '\0', POPT_ARG_STRING, &evolution_debug_log, 0, 
		  N_("Send the debugging output of all components to a file."), NULL },
		{ "setup-only", '\0', POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
		  &setup_only, 0, NULL, NULL },
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};
	GSList *uri_list;
	GValue popt_context_value = { 0, };
	GnomeProgram *program;
	poptContext popt_context;
	const char **args;
	char *evolution_directory;

	/* Make ElectricFence work.  */
	free (malloc (10));

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv, 
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      GNOME_PARAM_POPT_TABLE, options,
				      GNOME_PARAM_HUMAN_READABLE_NAME, _("Evolution"),
				      NULL);

	if (start_online && start_offline) {
		fprintf (stderr, _("%s: --online and --offline cannot be used together.\n  Use %s --help for more information.\n"),
			 argv[0], argv[0]);
		exit (1);
	}

	if (killev) {
		execl (EVOLUTION_TOOLSDIR "/killev", "killev", NULL);
		/* Not reached */
		exit (0);
	}

	setup_segv_redirect ();
	
	if (evolution_debug_log) {
		int fd;

		fd = open (evolution_debug_log, O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (fd) {
			dup2 (fd, STDOUT_FILENO);
			dup2 (fd, STDERR_FILENO);
			close (fd);
		} else
			g_warning ("Could not set up debugging output file.");
	}

	glade_init ();
	e_cursors_init ();
	e_icon_factory_init ();

	gnome_window_icon_set_default_from_file (EVOLUTION_IMAGES "/evolution-inbox.png");

	/* FIXME We shouldn't be using the old directory at all I think */
	evolution_directory = g_build_filename (g_get_home_dir (), "evolution", NULL);
	if (setup_only)
		exit (0);

	g_free (evolution_directory);

	uri_list = NULL;

	g_value_init (&popt_context_value, G_TYPE_POINTER);
	g_object_get_property (G_OBJECT (program), GNOME_PARAM_POPT_CONTEXT, &popt_context_value);
	popt_context = g_value_get_pointer (&popt_context_value);
	args = poptGetArgs (popt_context);
	if (args != NULL) {
		const char **p;

		for (p = args; *p != NULL; p++)
			uri_list = g_slist_prepend (uri_list, (char *) *p);
	}
	uri_list = g_slist_reverse (uri_list);
	g_value_unset (&popt_context_value);

	e_config_upgrade (evolution_directory);

	g_idle_add (idle_cb, uri_list);

	bonobo_main ();

	return 0;
}
