/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * subscribe-control-factory.c: A Bonobo Control factory for Subscribe Controls
 *
 * Author:
 *   Chris Toshok (toshok@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>

#include <gnome.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h> 
#include <bonobo/bonobo-ui-component.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>

#include "subscribe-control-factory.h"

#include "subscribe-control.h"
#include "mail.h"
#include "shell/Evolution.h"
#include "mail-config.h"
#include "mail-ops.h"

/* The Subscribe BonoboControls we have.  */
static GList *control_list = NULL;

/*
 * Add with 'subscribe_control'
 */
static BonoboUIVerb verbs [] = {
	/* Edit Menu */
	BONOBO_UI_VERB ("EditSelectAll", subscribe_select_all),
	BONOBO_UI_VERB ("EditUnSelectAll", subscribe_unselect_all),
	
	/* Folder Menu */
	BONOBO_UI_VERB ("SubscribeFolder", subscribe_folder),
	BONOBO_UI_VERB ("UnsubscribeFolder", unsubscribe_folder),

	/* Toolbar Specific */
	BONOBO_UI_VERB ("RefreshList", subscribe_refresh_list),

	BONOBO_UI_VERB_END
};

static void
set_pixmap (Bonobo_UIContainer container,
	    const char        *xml_path,
	    const char        *icon)
{
	char *path;
	GdkPixbuf *pixbuf;

	path = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution/buttons", icon);

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_return_if_fail (pixbuf != NULL);

	bonobo_ui_util_set_pixbuf (container, xml_path, pixbuf);

	gdk_pixbuf_unref (pixbuf);

	g_free (path);
}

static void
update_pixmaps (Bonobo_UIContainer container)
{
	set_pixmap (container, "/Toolbar/SubscribeFolder", "fetch-mail.png"); /* XXX */
	set_pixmap (container, "/Toolbar/UnsubscribeFolder", "compose-message.png"); /* XXX */
	set_pixmap (container, "/Toolbar/RefreshList", "forward.png"); /* XXX */
}

static GtkWidget*
make_folder_search_widget (GtkSignalFunc start_search_func,
			   gpointer user_data_for_search)
{
	GtkWidget *search_vbox = gtk_vbox_new (FALSE, 0);
	GtkWidget *search_entry = gtk_entry_new ();

	if (start_search_func) {
		gtk_signal_connect (GTK_OBJECT (search_entry), "activate",
				    start_search_func,
				    user_data_for_search);
	}
	
	/* add the search entry to the our search_vbox */
	gtk_box_pack_start (GTK_BOX (search_vbox), search_entry,
			    FALSE, TRUE, 3);
	gtk_box_pack_start (GTK_BOX (search_vbox),
			    gtk_label_new(_("Display Folders containing")),
			    FALSE, TRUE, 0);

	return search_vbox;
}

static void
control_activate (BonoboControl *control, BonoboUIHandler *uih,
		  SubscribeControl *sc)
{
	GtkWidget         *subscribe_control;
	BonoboUIComponent *component;
	Bonobo_UIContainer container;
	GtkWidget         *folder_search_widget;
	BonoboControl     *search_control;

	container = bonobo_control_get_remote_ui_handler (control);
	bonobo_ui_handler_set_container (uih, container);
	bonobo_object_release_unref (container, NULL);

	g_assert (container == bonobo_ui_compat_get_container (uih));
	g_return_if_fail (container != CORBA_OBJECT_NIL);
		
	subscribe_control = bonobo_control_get_widget (control);

	component = bonobo_ui_compat_get_component (uih);
	bonobo_ui_component_add_verb_list_with_data (
		component, verbs, subscribe_control);

	bonobo_ui_container_freeze (container, NULL);

	bonobo_ui_util_set_ui (
		component, container,
		EVOLUTION_DATADIR, "evolution-subscribe.xml",
		"evolution-mail");

	update_pixmaps (container);

	folder_search_widget = make_folder_search_widget (subscribe_search, sc);
	gtk_widget_show_all (folder_search_widget);
	search_control = bonobo_control_new (folder_search_widget);

	bonobo_ui_container_object_set (container,
					"/Toolbar/FolderSearch",
					bonobo_object_corba_objref (BONOBO_OBJECT (search_control)),
					NULL);
					
	bonobo_ui_container_thaw (container, NULL);
}

static void
control_deactivate (BonoboControl *control,
		    BonoboUIHandler *uih,
		    SubscribeControl *subscribe)
{
	g_warning ("In subscribe control_deactivate");
	bonobo_ui_component_rm (
		bonobo_ui_compat_get_component (uih),
		bonobo_ui_compat_get_container (uih), "/", NULL);

 	bonobo_ui_handler_unset_container (uih);
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
		control_activate (control, uih, user_data);
	else
		control_deactivate (control, uih, user_data);
}

static void
control_destroy_cb (BonoboControl *control,
		    gpointer       user_data)
{
	GtkWidget *subscribe_control = user_data;

	control_list = g_list_remove (control_list, control);

	gtk_object_destroy (GTK_OBJECT (subscribe_control));
}

BonoboControl *
subscribe_control_factory_new_control (const char *uri,
				       const Evolution_Shell shell)
{
	BonoboControl *control;
	GtkWidget *subscribe_control;

	subscribe_control = subscribe_control_new (shell);
	if (subscribe_control == NULL)
		return NULL;

	if (!subscribe_control_set_uri (SUBSCRIBE_CONTROL (subscribe_control), uri)) {
		gtk_object_sink (GTK_OBJECT (subscribe_control));
		return NULL;
	}

	gtk_widget_show (subscribe_control);
	
	control = bonobo_control_new (subscribe_control);
	
	if (control == NULL) {
		gtk_object_destroy (GTK_OBJECT (subscribe_control));
		return NULL;
	}
	
	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    control_activate_cb, subscribe_control);

	gtk_signal_connect (GTK_OBJECT (control), "destroy",
			    control_destroy_cb, subscribe_control);

	control_list = g_list_prepend (control_list, control);

	return control;
}

GList *
subscribe_control_factory_get_control_list (void)
{
	return control_list;
}
