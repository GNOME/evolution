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


static const gchar *warning_dialog_buttons[] = {
	"Cancel",
	"OK",
	NULL
};
				       
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
	
	if (!warning_result)
		bonobo_control_set_property_bag (
			 control,
			 FOLDER_BROWSER (folder_browser)->properties);
	
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
			"GOADID:Evolution:FolderBrowserFactory:1.0",
			folder_browser_factory, NULL);

	if (bonobo_folder_browser_factory == NULL){
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("We are sorry, Evolution's Folder Browser can not be initialized.")); 
		exit (1);
	}
}
