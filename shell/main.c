/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* main.c
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

#include <config.h>

#include "e-util/e-gtk-utils.h"

#include "e-icon-factory.h"
#include "e-shell-constants.h"
#include "e-shell-config.h"
#include "e-setup.h"

#include "e-shell.h"

#include <gconf/gconf-client.h>

#include <gtk/gtkalignment.h>
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

#ifdef GTKHTML_HAVE_GCONF
#include <gconf/gconf.h>
#endif

#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-cursors.h>

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


static EShell *shell = NULL;
static char *evolution_directory = NULL;

/* Command-line options.  */
static gboolean no_splash = FALSE;
static gboolean start_online = FALSE;
static gboolean start_offline = FALSE;
static gboolean force_upgrade = FALSE;

extern char *evolution_debug_log;


static GtkWidget *
quit_box_new (void)
{
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *frame;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, FALSE, FALSE);
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
no_views_left_cb (EShell *shell, gpointer data)
{
	GtkWidget *quit_box;

	quit_box = quit_box_new ();
	g_object_add_weak_pointer (G_OBJECT (quit_box), (void **) &quit_box);

	/* FIXME: This is wrong.  We should exit only when the shell is
	   destroyed.  But refcounting is broken at present, so this is a
	   reasonable workaround for now.  */

	e_shell_unregister_all (shell);

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

	client = gconf_client_get_default ();

	if (gconf_client_get_bool (client, "/apps/evolution/shell/skip_warning_dialog", NULL)) {
		g_object_unref (client);
		return;
	}

	g_object_unref (client);

	warning_dialog = gtk_dialog_new_with_buttons("Ximian Evolution " VERSION, parent,
						     GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	label = gtk_label_new (
		/* xgettext:no-c-format */
		_("Hi.  Thanks for taking the time to download this preview release\n"
		  "of the Ximian Evolution groupware suite.\n"
		  "\n"
		  "This version of Ximian Evolution is not yet complete. It's getting close,\n"
		  "but some features are either unfinished or don't work properly.\n"
		  "\n"
		  "If you want a stable version of Evolution, we urge you to uninstall,\n"
		  "this version, and install a 1.0.x version instead (1.0.8)\n"
		  "\n"
		  "If you find bugs, please report them to us at bugzilla.ximian.com.\n"
                  "This product comes with no warranty and is not intended for\n"
		  "individuals prone to violent fits of anger.\n"
                  "\n"
		  "We hope that you enjoy the results of our hard work, and we\n"
		  "eagerly await your contributions!\n"
			));
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
view_map_callback (GtkWidget *widget,
		   void *data)
{
	gtk_signal_disconnect_by_func (GTK_OBJECT (widget),
				       G_CALLBACK (view_map_callback),
				       data);

	show_development_warning (GTK_WINDOW (widget));
}

static void
new_view_created_callback (EShell *shell,
			   EShellView *view,
			   void *data)
{
	gtk_signal_disconnect_by_func (GTK_OBJECT (shell),
				       G_CALLBACK (new_view_created_callback),
				       data);

	g_signal_connect (view, "map", G_CALLBACK (view_map_callback), NULL);
}


static void
upgrade_from_1_0_if_needed (void)
{
#if 0
	EConfigListener *config_listener;
	int result;

	config_listener = e_config_listener_new ();

	if (! force_upgrade
	    && e_config_listener_get_boolean_with_default (config_listener, "/Shell/upgrade_from_1_0_to_1_2_performed",
							   FALSE, NULL))
		return;

	g_print ("\nOlder configuration files detected, upgrading...\n");

	result = system (PREFIX "/bin/evolution-mail-upgrade");

	if (result == 0)
		g_print ("\n--> Configuration files upgraded from version 1.0.\n");
	else
		g_print ("\n*** Error upgrading configuration files -- status %d\n", result);

	e_config_listener_set_boolean (config_listener, "/Shell/upgrade_from_1_0_to_1_2_performed", TRUE);

	g_object_unref (config_listener);
#endif /* FIXME */
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
	gboolean display_default;
	gboolean displayed_any;

	upgrade_from_1_0_if_needed ();

	CORBA_exception_init (&ev);

	uri_list = (GSList *) data;

	if (! start_online && ! start_offline)
		startup_line_mode = E_SHELL_STARTUP_LINE_MODE_CONFIG;
	else if (start_online)
		startup_line_mode = E_SHELL_STARTUP_LINE_MODE_ONLINE;
	else
		startup_line_mode = E_SHELL_STARTUP_LINE_MODE_OFFLINE;

	shell = e_shell_new (evolution_directory, ! no_splash, startup_line_mode, &result);
	g_free (evolution_directory);

	switch (result) {
	case E_SHELL_CONSTRUCT_RESULT_OK:
		e_shell_config_factory_register (shell);

		g_signal_connect (shell, "no_views_left", G_CALLBACK (no_views_left_cb), NULL);
		g_object_weak_ref (G_OBJECT (shell), shell_weak_notify, NULL);

		if (!getenv ("EVOLVE_ME_HARDER"))
			g_signal_connect (shell, "new_view_created",
					  G_CALLBACK (new_view_created_callback), NULL);

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

	have_evolution_uri = FALSE;
	displayed_any = FALSE;

	for (p = uri_list; p != NULL; p = p->next) {
		const char *uri;

		uri = (const char *) p->data;
		if (strncmp (uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN) == 0 ||
		    strncmp (uri, E_SHELL_DEFAULTURI_PREFIX, E_SHELL_DEFAULTURI_PREFIX_LEN) == 0)
			have_evolution_uri = TRUE;
	}

	if (shell == NULL) {
		/* We're talking to a remote shell. If the user didn't ask us to open any particular
 		   URI, then open another view of the default URI.  */
		if (uri_list == NULL)
			display_default = TRUE;
		else
			display_default = FALSE;
	} else {
		/* We're starting a new shell. If the user didn't specify any evolution: URIs to
		   view, AND we can't load the user's previous settings, then show the default
		   URI.  */
		if (! have_evolution_uri) {
			e_shell_create_view (shell, NULL, NULL);
			display_default = TRUE;
			displayed_any = TRUE;
		} else {
			display_default = FALSE;
		}
	}

	for (p = uri_list; p != NULL; p = p->next) {
		const char *uri;

		uri = (const char *) p->data;
		GNOME_Evolution_Shell_handleURI (corba_shell, uri, &ev);
		if (ev._major == CORBA_NO_EXCEPTION)
			displayed_any = TRUE;
		else {
			g_warning ("CORBA exception %s when requesting URI -- %s",
				   BONOBO_EX_REPOID (&ev), uri);
			CORBA_exception_free (&ev);
		}
	}

	g_slist_free (uri_list);

	if (display_default && ! displayed_any) {
		const char *uri;

		uri = E_SHELL_VIEW_DEFAULT_URI;
		GNOME_Evolution_Shell_handleURI (corba_shell, uri, &ev);
		if (ev._major != CORBA_NO_EXCEPTION)
			g_warning ("CORBA exception %s when requesting URI -- %s", BONOBO_EX_REPOID (&ev), uri);
	}

	CORBA_Object_release (corba_shell, &ev);

	CORBA_exception_free (&ev);
	
	if (shell == NULL)
		bonobo_main_quit ();

	return FALSE;
}

int
main (int argc, char **argv)
{
	struct poptOption options[] = {
		{ "no-splash", '\0', POPT_ARG_NONE, &no_splash, 0,
		  N_("Disable splash screen"), NULL },
		{ "offline", '\0', POPT_ARG_NONE, &start_offline, 0, 
		  N_("Start in offline mode"), NULL },
		{ "online", '\0', POPT_ARG_NONE, &start_online, 0, 
		  N_("Start in online mode"), NULL },
		{ "debug", '\0', POPT_ARG_STRING, &evolution_debug_log, 0, 
		  N_("Send the debugging output of all components to a file."), NULL },
		{ "force-upgrade", '\0', POPT_ARG_NONE, &force_upgrade, 0, 
		  N_("Force upgrading of configuration files from Evolution 1.0.x"), NULL },
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};
	GSList *uri_list;

	/* Make ElectricFence work.  */
	free (malloc (10));

	gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv, 
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_POPT_TABLE, options,
			    GNOME_PARAM_HUMAN_READABLE_NAME, _("Evolution"),
			    NULL);

	if (start_online && start_offline) {
		fprintf (stderr, _("%s: --online and --offline cannot be used together.\n  Use %s --help for more information.\n"),
			 argv[0], argv[0]);
		exit (1);
	}

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

	glade_gnome_init ();
	e_cursors_init ();
	e_icon_factory_init ();

	gnome_window_icon_set_default_from_file (EVOLUTION_IMAGES "/evolution-inbox.png");

	/* FIXME */
	evolution_directory = g_concat_dir_and_file (g_get_home_dir (), "evolution");

	if (! e_setup (evolution_directory))
		exit (1);

	uri_list = NULL;

#if 0
	args = poptGetArgs (popt_context);
	if (args != NULL) {
		const char **p;

		for (p = args; *p != NULL; p++)
			uri_list = g_slist_prepend (uri_list, (char *) *p);
	}
#endif

	gtk_idle_add (idle_cb, uri_list);

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	bonobo_main ();

	return 0;
}
