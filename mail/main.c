/*
 * main.c: The core of the mail component
 *
 * Author:
 *   Miguel de Icaza (miguel@ximian.com)
 *
 * (C) 2000 Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>

#include <libgnome/gnome-sound.h>
#include <bonobo/bonobo-main.h>
#include <bonobo-activation/bonobo-activation-init.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs.h>

#include <gconf/gconf.h>

#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-cursors.h>
#include <gal/widgets/e-unicode.h>

#include "e-util/e-passwords.h"
#include "e-util/e-proxy.h"

#include "component-factory.h"
#include "composer/evolution-composer.h"
#include "mail.h"
#include "mail-mt.h"

/*#define DO_MCHECK*/

#ifdef DO_MCHECK
static int blowup(int status)
{
	switch(status) {
	case 1:
		printf("Double free failure\n");
		break;
	case 2:
		printf("Memory clobbered before block\n");
		break;
	case 3:
		printf("Memory clobbered after block\n");
		break;
	}
	abort();
	return status;
}
#endif

/* The GNOME SEGV handler will lose if it's not run from the main Gtk
 * thread. So if we crash in another thread, redirect the signal.
 */
static void (*gnome_segv_handler) (int);

static GStaticMutex segv_mutex = G_STATIC_MUTEX_INIT;

static void
segv_redirect (int sig)
{
	if (pthread_self () == mail_gui_thread)
		gnome_segv_handler (sig);
	else {
		pthread_kill (mail_gui_thread, sig);
		/* We can't return from the signal handler or the
		 * thread may SEGV again. But we can't pthread_exit,
		 * because then the thread may get cleaned up before
		 * bug-buddy can get a stack trace. So we block by
		 * trying to lock a mutex we know is already locked.
		 */
		g_static_mutex_lock (&segv_mutex);
	}
}

int
main (int argc, char *argv [])
{
	CORBA_ORB orb;
	struct sigaction sa, osa;

        /* used to make elfence work */                                         
	free(malloc (10));
#ifdef DO_MCHECK                                                                
        /*mtrace();*/                                                           
        mcheck(blowup);                                                         
#endif                                                                          
	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	g_thread_init (NULL);

	gnome_init_with_popt_table ("evolution-mail-component", VERSION,
				    argc, argv, bonobo_activation_popt_options, 0, NULL);
	
	sigaction (SIGSEGV, NULL, &osa);
	if (osa.sa_handler != SIG_DFL) {
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
	
	if (!bonobo_init (&argc, argv)) {
		g_error ("Mail component could not initialize Bonobo.\n"
			 "If there was a warning message about the "
			 "RootPOA, it probably means\nyou compiled "
			 "Bonobo against GOAD instead of Bonobo Activation.");
	}
	
	gconf_init (argc, argv, NULL);
	
	glade_gnome_init ();
	
	gnome_vfs_init ();
	
	e_cursors_init ();
	
	e_proxy_init ();
	
	mail_config_init ();
	mail_msg_init ();
	
	gnome_sound_init ("localhost");
	
	component_factory_init ();
	evolution_composer_factory_init (composer_send_cb, composer_save_draft_cb);
	
	if (gdk_threads_mutex) {
		g_mutex_free (gdk_threads_mutex);
		gdk_threads_mutex = NULL;
	}
	
	g_print ("Evolution Mail ready and running.\n");
	
	GDK_THREADS_ENTER ();
	bonobo_main ();
	
	mail_msg_cleanup();
	
	GDK_THREADS_LEAVE ();
	
	mail_config_write_on_exit ();
	
	e_passwords_shutdown ();
	
	gnome_sound_shutdown ();
	
	return 0;
}
