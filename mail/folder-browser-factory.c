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

#include "folder-browser-factory.h"

#include "folder-browser.h"
#include "mail.h"
#include "shell/Evolution.h"
#include "mail-config.h"

/* The FolderBrowser BonoboControls we have.  */
static GList *control_list = NULL;

static GnomeUIInfo gnome_toolbar [] = {
	GNOMEUIINFO_ITEM_STOCK (N_("Get mail"), N_("Check for new mail"), fetch_mail, GNOME_STOCK_PIXMAP_MAIL_RCV),
	GNOMEUIINFO_ITEM_STOCK (N_("Compose"), N_("Compose a new message"), compose_msg, GNOME_STOCK_PIXMAP_MAIL_NEW),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (N_("Reply"), N_("Reply to the sender of this message"), reply_to_sender, GNOME_STOCK_PIXMAP_MAIL_RPL),
	GNOMEUIINFO_ITEM_STOCK (N_("Reply to All"), N_("Reply to all recipients of this message"), reply_to_all, GNOME_STOCK_PIXMAP_MAIL_RPL),

	GNOMEUIINFO_ITEM_STOCK (N_("Forward"), N_("Forward this message"), forward_msg, GNOME_STOCK_PIXMAP_MAIL_FWD),

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (N_("Move"), N_("Move message to a new folder"), move_msg, GNOME_STOCK_PIXMAP_MAIL_SND),

	GNOMEUIINFO_ITEM_STOCK (N_("Print"), N_("Print the selected message"), print_msg, GNOME_STOCK_PIXMAP_PRINT),

	GNOMEUIINFO_ITEM_STOCK (N_("Delete"), N_("Delete this message"), delete_msg, GNOME_STOCK_PIXMAP_TRASH),

	GNOMEUIINFO_END
};

static void
control_activate (BonoboControl *control, BonoboUIHandler *uih,
		  FolderBrowser *fb)
{
	Bonobo_UIHandler  remote_uih;
	BonoboControl *toolbar_control;
	GnomeDockItemBehavior behavior;
	GtkWidget *toolbar, *toolbar_frame, *folder_browser;
	char *toolbar_name = g_strdup_printf ("/Toolbar%d", fb->serial);

	remote_uih = bonobo_control_get_remote_ui_handler (control);
	bonobo_ui_handler_set_container (uih, remote_uih);
	bonobo_object_release_unref (remote_uih, NULL);

	folder_browser = bonobo_control_get_widget (control);

	bonobo_ui_handler_menu_new_toggleitem (uih, "/View/Threaded",
					       _("_Threaded Message List"),
					       NULL, -1, 0, 0, NULL, NULL);
	bonobo_ui_handler_menu_set_toggle_state (uih, "/View/Threaded",
						 mail_config_thread_list());
	bonobo_ui_handler_menu_set_callback (uih, "/View/Threaded",
					     message_list_toggle_threads,
					     FOLDER_BROWSER (folder_browser)->message_list,
					     NULL);
	
	bonobo_ui_handler_menu_new_item (uih, "/File/<Print Placeholder>/Print message...",
					 _("_Print message"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_PRINT,
					 0, 0, (void *) print_msg, folder_browser);

	bonobo_ui_handler_menu_new_separator (uih, "/File/<Print Placeholder>/separator1", -1);

	bonobo_ui_handler_menu_new_item (uih, "/Actions/Edit Message", _("E_dit Message"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_MAIL_NEW,
					 0, 0, edit_message, folder_browser);
	
	bonobo_ui_handler_menu_new_item (uih, "/Actions/View Message", 
					 _("_View Message"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_MAIL_NEW,
					 0, 0, view_message, folder_browser);

  	bonobo_ui_handler_menu_new_separator (uih, "/Actions/separator1", -1);

	bonobo_ui_handler_menu_new_item (uih, "/Actions/Mark all seen",
					 _("_Mark all messages seen"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE, NULL,
					 0, 0, mark_all_seen, folder_browser);	
	bonobo_ui_handler_menu_new_item (uih, "/Actions/Expunge", _("_Expunge"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_STOCK,
					 GNOME_STOCK_MENU_TRASH,
					 0, 0, expunge_folder, folder_browser);

	bonobo_ui_handler_menu_new_item (uih, "/Tools/Mail Filters ...", _("Mail _Filters ..."),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE,
					 0,
					 0, 0, filter_edit, folder_browser);

	bonobo_ui_handler_menu_new_item (uih, "/Tools/vFolder Editor ...", _("_vFolder Editor ..."),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE,
					 0,
					 0, 0, vfolder_edit_vfolders, folder_browser);

	bonobo_ui_handler_menu_new_item (uih, "/Tools/Mail Configuration ...", _("_Mail Configuration ..."),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE,
					 0,
					 0, 0, (void *) mail_config, NULL);
	
	bonobo_ui_handler_menu_new_item (uih, "/Tools/Forget Passwords", _("Forget _Passwords"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE,
					 0,
					 0, 0, forget_passwords, NULL);
	

	bonobo_ui_handler_menu_new_item (uih, "/Tools/Configure Folder", _("_Configure Folder"),
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE,
					 0,
					 0, 0, configure_folder, folder_browser);
	
	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL,
				   GTK_TOOLBAR_BOTH);

	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (toolbar),
					  gnome_toolbar,
					  NULL, folder_browser);

	gtk_widget_show_all (toolbar);

	behavior = GNOME_DOCK_ITEM_BEH_EXCLUSIVE |
                    GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL;
	if (!gnome_preferences_get_toolbar_detachable ())
		behavior |= GNOME_DOCK_ITEM_BEH_LOCKED;

	toolbar_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (toolbar_frame), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (toolbar_frame), toolbar);
	gtk_widget_show (toolbar_frame);

	gtk_widget_show_all (toolbar_frame);

	toolbar_control = bonobo_control_new (toolbar_frame);
	bonobo_ui_handler_dock_add (uih, toolbar_name,
				    bonobo_object_corba_objref (BONOBO_OBJECT (toolbar_control)),
				    behavior,
				    GNOME_DOCK_TOP,
				    1, 1, 0);
	g_free (toolbar_name);
}

static void
control_deactivate (BonoboControl *control,
		    BonoboUIHandler *uih,
		    FolderBrowser *fb)
{
	char *toolbar_name = g_strdup_printf ("/Toolbar%d", fb->serial);

	bonobo_ui_handler_menu_remove (uih, "/File/<Print Placeholder>/separator1");
	bonobo_ui_handler_menu_remove (uih, "/File/<Print Placeholder>/Print message...");

	bonobo_ui_handler_menu_remove (uih, "/View/Threaded");
	bonobo_ui_handler_menu_remove (uih, "/Actions/Mark all seen");
	bonobo_ui_handler_menu_remove (uih, "/Actions/Edit Message");
	bonobo_ui_handler_menu_remove (uih, "/Actions/View Message");
	bonobo_ui_handler_menu_remove (uih, "/Actions/Expunge");
	bonobo_ui_handler_menu_remove (uih, "/Tools/Mail Filters ...");
	bonobo_ui_handler_menu_remove (uih, "/Tools/vFolder Editor ...");
	bonobo_ui_handler_menu_remove (uih, "/Tools/Mail Configuration ...");
	bonobo_ui_handler_menu_remove (uih, "/Tools/Forget Passwords");
	bonobo_ui_handler_menu_remove (uih, "/Tools/Configure Folder");
	bonobo_ui_handler_dock_remove (uih, toolbar_name);
	g_free (toolbar_name);
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
	GtkWidget *folder_browser = user_data;

	control_list = g_list_remove (control_list, control);

	gtk_object_destroy (GTK_OBJECT (folder_browser));
}

BonoboControl *
folder_browser_factory_new_control (const char *uri)
{
	BonoboControl *control;
	GtkWidget *folder_browser;

	folder_browser = folder_browser_new ();
	if (folder_browser == NULL)
		return NULL;

	if (!folder_browser_set_uri (FOLDER_BROWSER (folder_browser), uri)) {
		gtk_object_sink (GTK_OBJECT (folder_browser));
		return NULL;
	}

	gtk_widget_show (folder_browser);
	
	control = bonobo_control_new (folder_browser);
	
	if (control == NULL) {
		gtk_object_destroy (GTK_OBJECT (folder_browser));
		return NULL;
	}
	
	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    control_activate_cb, folder_browser);

	gtk_signal_connect (GTK_OBJECT (control), "destroy",
			    control_destroy_cb, folder_browser);

	control_list = g_list_prepend (control_list, control);

	return control;
}

GList *
folder_browser_factory_get_control_list (void)
{
	return control_list;
}
