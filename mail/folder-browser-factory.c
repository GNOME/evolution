/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * folder-browser-factory.c: A Bonobo Control factory for Folder Browsers
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h> 
#include "e-util/e-util.h"
#include "e-util/e-gui-utils.h"
#include "folder-browser.h"
#include "main.h"
#include "shell/Evolution.h"
#include "shell/evolution-service-repository.h"
#include "composer/e-msg-composer.h"


static const gchar *warning_dialog_buttons[] = {
	"Cancel",
	"OK",
	NULL
};
			
static void
folder_browser_set_shell (EvolutionServiceRepository *sr,
			  Evolution_Shell shell, 
			  void *closure)
{
	FolderBrowser *folder_browser;
	CORBA_Environment ev;

	g_return_if_fail (closure);
	g_return_if_fail (IS_FOLDER_BROWSER (closure));
	g_return_if_fail (shell != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	folder_browser = FOLDER_BROWSER (closure);
	printf ("I AM A FOLDER BROWSER AND I AM STORING THE SHELL\n");
	/* FIXME : ref the shell here */
	folder_browser->shell = shell;

	/* test the component->shell registration */
	Evolution_Shell_register_service (shell, Evolution_Shell_MAIL_STORE, "a_service", &ev);
}

static void 
folder_browser_control_add_service_repository_interface (BonoboControl *control,
							 GtkWidget *folder_browser)
{
	EvolutionServiceRepository *sr;

	/* 
	 * create an implementation for the Evolution::ServiceRepository
	 * interface
	 */
	sr = evolution_service_repository_new (folder_browser_set_shell,
					       (void *)folder_browser);
	
	/* add the interface to the control */
	bonobo_object_add_interface (BONOBO_OBJECT (control), 
				     BONOBO_OBJECT (sr));
}


static int
development_warning ()
{
	gint result;
	GtkWidget *label, *warning_dialog;

	warning_dialog = gnome_dialog_new ("Don't do that",
				   "I know what I'm doing,\nI want to crash my mail files",
				   "I'll try it later",
				   NULL);

	label = gtk_label_new ("This is a developement version of Evolution.\n "
			       "Using the mail component on your mail files\n "
			       "is extremely hazardous.\n"
			       "Please backup all your mails before trying\n "
			       "this program. \n     You have been warned\n");
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (warning_dialog)->vbox), 
			    label, TRUE, TRUE, 0);

	result = gnome_dialog_run (GNOME_DIALOG (warning_dialog));
	
	gtk_object_destroy (GTK_OBJECT (label));
	gtk_object_destroy (GTK_OBJECT (warning_dialog));

	return result;
	
} 

static void
msg_composer_send_cb (EMsgComposer *composer,
		      gpointer data)
{
	CamelMimeMessage *message;
	CamelStream *stream;
	gint stdout_dup;

	message = e_msg_composer_get_message (composer);

	stdout_dup = dup (1);
	stream = camel_stream_fs_new_with_fd (stdout_dup);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message),
					    stream);
	camel_stream_close (stream);

	gtk_object_unref (GTK_OBJECT (message));

#if 0
	gtk_widget_destroy (GTK_WIDGET (composer));
	gtk_main_quit ();
#endif
}


static void 
msg_composer_cb (GtkObject *obj, gpointer user_data)
{
	CamelMimeMessage *msg;
	GtkWidget *composer;

	composer = e_msg_composer_new ();
	gtk_signal_connect (GTK_OBJECT (composer), "send", GTK_SIGNAL_FUNC (msg_composer_send_cb), NULL);
	gtk_widget_show (composer);
}


static void 
control_add_menu (BonoboControl *control)
{
	Bonobo_UIHandler  remote_uih;
	BonoboUIHandler  *uih;

	uih = bonobo_control_get_ui_handler (control);
	g_assert (uih);
	
	remote_uih = bonobo_control_get_remote_ui_handler (control);
	bonobo_ui_handler_set_container (uih, remote_uih);		
	
	bonobo_ui_handler_menu_new_item (uih,
					 "/File/New", N_("_Mail"), NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL, 0, 0,
					 msg_composer_cb, NULL);
	
}


static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     gpointer user_data)
{
	control_add_menu (control);
	
}



/*
 * Creates the Folder Browser, wraps it in a Bonobo Control, and
 * sets the Bonobo Control properties to point to the Folder Browser
 * Properties
 */
static BonoboObject *
folder_browser_factory (BonoboGenericFactory *factory, void *closure)
{
	BonoboControl *control;
	GtkWidget *folder_browser;
	gint warning_result;


	warning_result = development_warning ();
	
	if (warning_result) 
		folder_browser = gtk_label_new ("This should be the mail component");
	else {
		folder_browser = folder_browser_new ();
		folder_browser_set_uri (FOLDER_BROWSER (folder_browser), "inbox");
	}
	
	if (folder_browser == NULL)
		return NULL;

	gtk_widget_show(folder_browser);
	
	control = bonobo_control_new (folder_browser);
	
	if (control == NULL){
		gtk_object_destroy (GTK_OBJECT (folder_browser));
		return NULL;
	}
	
	gtk_signal_connect (GTK_OBJECT (control), "activate", control_activate_cb, NULL);
	


	
	bonobo_control_set_property_bag (control,
					 FOLDER_BROWSER (folder_browser)->properties);

	/* for the moment, the control has the ability to register 
	 * some services itself, but this should not last. 
	 * 
	 * It's not the way to do it, but we don't have the 
	 * correct infrastructure in the shell now.    
	 */
	folder_browser_control_add_service_repository_interface (control, folder_browser); 	       	
	
	
	return BONOBO_OBJECT (control);

}

void
folder_browser_factory_init (void)
{
	static BonoboGenericFactory *bonobo_folder_browser_factory = NULL;
	
	
	if (bonobo_folder_browser_factory != NULL)
		return;

	bonobo_folder_browser_factory =
		bonobo_generic_factory_new (
			"control-factory:evolution-mail",
			folder_browser_factory, NULL);

	if (bonobo_folder_browser_factory == NULL){
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("We are sorry, Evolution's Folder Browser can not be initialized.")); 
		exit (1);
	}
}
