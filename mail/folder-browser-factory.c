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
#include <camel/camel-stream-fs.h>
#include "mail-ops.h"

#ifdef USING_OAF
#define CONTROL_FACTORY_ID "OAFIID:control-factory:evolution-mail:25902062-543b-4f44-8702-d90145fcdbf2"
#else
#define CONTROL_FACTORY_ID "control-factory:evolution-mail"
#endif
			
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

	folder_browser->shell = shell;

	/* test the component->shell registration */
	Evolution_Shell_register_service (shell, Evolution_Shell_MAIL_STORE, "a_service", &ev);

	CORBA_exception_free (&ev);
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

	warning_dialog = gnome_dialog_new (
		"Don't do that",
		"I know what I'm doing,\nI want to lose mail!",
		"I'll try it later",
		NULL);

	label = gtk_label_new (
		_("This is a development version of Evolution.\n"
		  "Using the mail component on your mail files\n"
		  "is extremely hazardous.\n\n"
		  "Do not run this program on your real mail\n "
		  "and do not give it access to your real mail server.\n\n"
		  "You have been warned\n"));
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (warning_dialog)->vbox), 
			    label, TRUE, TRUE, 0);

	result = gnome_dialog_run (GNOME_DIALOG (warning_dialog));
	
	gtk_object_destroy (GTK_OBJECT (label));
	gtk_object_destroy (GTK_OBJECT (warning_dialog));

	return result;
} 

static void
random_cb (GtkWidget *button, gpointer user_data)
{
	printf ("Yow! I am called back!\n");
}

static GnomeUIInfo gnome_toolbar [] = {
	GNOMEUIINFO_ITEM_STOCK (N_("New mail"), N_("Check for new mail"), fetch_mail, GNOME_STOCK_PIXMAP_MAIL_RCV),
	GNOMEUIINFO_ITEM_STOCK (N_("Send"), N_("Send a new message"), send_msg, GNOME_STOCK_PIXMAP_MAIL_SND),
	GNOMEUIINFO_ITEM_STOCK (N_("Find"), N_("Find messages"), random_cb, GNOME_STOCK_PIXMAP_SEARCH),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (N_("Reply"), N_("Reply to the sender of this message"), reply_to_sender, GNOME_STOCK_PIXMAP_MAIL_RPL),
	GNOMEUIINFO_ITEM_STOCK (N_("Reply to All"), N_("Reply to all recipients of this message"), reply_to_all, GNOME_STOCK_PIXMAP_MAIL_RPL),

	GNOMEUIINFO_ITEM_STOCK (N_("Forward"), N_("Forward this message"), forward_msg, GNOME_STOCK_PIXMAP_MAIL_FWD),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (N_("Print"), N_("Print the selected message"), random_cb, GNOME_STOCK_PIXMAP_PRINT),

	GNOMEUIINFO_ITEM_STOCK (N_("Delete"), N_("Delete this message"), random_cb, GNOME_STOCK_PIXMAP_TRASH),

	GNOMEUIINFO_END
};

static void
control_activate (BonoboControl *control, BonoboUIHandler *uih)
{
	Bonobo_UIHandler  remote_uih;
	BonoboControl *toolbar_control;
	GtkWidget *toolbar, *folder_browser;

	remote_uih = bonobo_control_get_remote_ui_handler (control);
	bonobo_ui_handler_set_container (uih, remote_uih);		

	bonobo_ui_handler_menu_new_item (uih, "/File/Mail", N_("_Mail"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, send_msg, NULL);

	folder_browser = bonobo_control_get_widget (control);

	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL,
				   GTK_TOOLBAR_BOTH);

	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (toolbar),
					  gnome_toolbar,
					  NULL, folder_browser);

	gtk_widget_show_all (toolbar);

	toolbar_control = bonobo_control_new (toolbar);
	bonobo_ui_handler_dock_add (uih, "/Toolbar",
				    bonobo_object_corba_objref (BONOBO_OBJECT (toolbar_control)),
				    GNOME_DOCK_ITEM_BEH_LOCKED |
				    GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
				    GNOME_DOCK_TOP,
				    1, 1, 0);
}

static void
control_deactivate (BonoboControl *control, BonoboUIHandler *uih)
{
	bonobo_ui_handler_menu_remove (uih, "/File/Mail");
	bonobo_ui_handler_dock_remove (uih, "/Toolbar");
}

static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     gpointer user_data)
{
	BonoboUIHandler  *uih;

	uih = bonobo_control_get_ui_handler (control);
	g_assert (uih);
	
	if (activate)
		control_activate (control, uih);
	else
		control_deactivate (control, uih);
}

static void
control_destroy_cb (BonoboControl *control,
		    gpointer       user_data)
{
	GtkWidget *folder_browser = user_data;

	gtk_object_destroy (GTK_OBJECT (folder_browser));
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
	gint warning_result = 0;


	if (!getenv ("EVOLVE_ME_HARDER"))
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
	
	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    control_activate_cb, NULL);

	gtk_signal_connect (GTK_OBJECT (control), "destroy",
			    control_destroy_cb, folder_browser);	
	
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

	bonobo_folder_browser_factory = bonobo_generic_factory_new (CONTROL_FACTORY_ID,
								    folder_browser_factory,
								    NULL);

	if (bonobo_folder_browser_factory == NULL){
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("We are sorry, Evolution's Folder Browser can not be initialized.")); 
		exit (1);
	}
}
