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
#include <bonobo/bonobo-ui-component.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>

#include "folder-browser-factory.h"

#include "folder-browser.h"
#include "mail.h"
#include "shell/Evolution.h"
#include "mail-config.h"
#include "mail-ops.h"

/* The FolderBrowser BonoboControls we have.  */
static GList *control_list = NULL;

static void
register_ondemand (RuleContext *f, FilterRule *rule, gpointer data)
{
	FolderBrowser *fb = FOLDER_BROWSER (data);
	BonoboUIHandler *uih = gtk_object_get_data (GTK_OBJECT (fb), "uih");
	gchar *text;
	struct fb_ondemand_closure *oc;

	oc = g_new (struct fb_ondemand_closure, 1);
	oc->rule = rule;
	oc->fb = fb;
	oc->path = g_strdup_printf ("/*Component Placeholder*/Folder/Filter-%s", rule->name);

	if (fb->filter_menu_paths == NULL)
		bonobo_ui_handler_menu_new_separator (uih, "/*Component Placeholder*/Folder/separator1", -1);

	text = g_strdup_printf (_("Run filter \"%s\""), rule->name);
	fb->filter_menu_paths = g_slist_prepend (fb->filter_menu_paths, oc);

	bonobo_ui_handler_menu_new_item (uih, oc->path, text,
					 NULL, -1,
					 BONOBO_UI_HANDLER_PIXMAP_NONE,
					 0,
					 0, 0, run_filter_ondemand, oc);
	g_free (text);
}

static void
create_ondemand_hooks (FolderBrowser *fb, BonoboUIHandler *uih)
{
	gchar *system, *user;

	user = g_strdup_printf ("%s/filters.xml", evolution_dir);
	system = EVOLUTION_DATADIR "/evolution/filtertypes.xml";
	fb->filter_context = filter_context_new();
	gtk_object_set_data (GTK_OBJECT (fb), "uih", uih);
	rule_context_load ((RuleContext *) fb->filter_context, system, user,
			   register_ondemand, fb);
	gtk_object_remove_data (GTK_OBJECT (fb), "uih");
	g_free (user);
}

/*
 * Add with 'folder_browser'
 */
BonoboUIVerb verbs [] = {
	BONOBO_UI_VERB ("PrintMessage", print_msg),

	/* Settings Menu */
	BONOBO_UI_VERB ("SetMailFilter", filter_edit),
	BONOBO_UI_VERB ("VFolderEdit", vfolder_edit_vfolders),
	BONOBO_UI_VERB ("SetMailConfig", providers_config),
	BONOBO_UI_VERB ("SetForgetPwd", forget_passwords),

	/* Message Menu */
	BONOBO_UI_VERB ("MessageOpenNewWnd", view_message),
	BONOBO_UI_VERB ("MessageEdit", edit_message),
	BONOBO_UI_VERB ("MessagePrint", print_msg),
	BONOBO_UI_VERB ("MessageReplySndr", reply_to_sender),
	BONOBO_UI_VERB ("MessageReplyAll", reply_to_all),
	BONOBO_UI_VERB ("MessageForward", forward_msg),
	BONOBO_UI_VERB ("MessageDelete", delete_msg),
	BONOBO_UI_VERB ("MessageMove", move_msg),
	BONOBO_UI_VERB ("MessageCopy", copy_msg),

	BONOBO_UI_VERB ("MessageVFolderSubj", vfolder_subject),
	BONOBO_UI_VERB ("MessageVFolderSndr", vfolder_sender),
	BONOBO_UI_VERB ("MessageVFolderRecip", vfolder_recipient),

	BONOBO_UI_VERB ("MessageFilterSubj", filter_subject),
	BONOBO_UI_VERB ("MessageFilderSndr", filter_sender),
	BONOBO_UI_VERB ("MessageFilderRecip", filter_recipient),

	/* Folder Menu */
	BONOBO_UI_VERB ("FolderMarkAllRead", mark_all_seen),
	BONOBO_UI_VERB ("FolderDeleteAll", mark_all_deleted),
	BONOBO_UI_VERB ("FolderExpunge", expunge_folder),
	BONOBO_UI_VERB ("FolderConfig", configure_folder),

	/* Toolbar specific */
	BONOBO_UI_VERB ("MailGet", send_receieve_mail),
	BONOBO_UI_VERB ("MailCompose", compose_msg),

	BONOBO_UI_VERB_END
};

static void
set_pixmap (Bonobo_UIContainer container,
	    const char        *xml_path,
	    const char        *icon)
{
	char *path, *parent_path;
	xmlNode *node;
	GdkPixbuf *pixbuf;

	path = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution/buttons", icon);

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_return_if_fail (pixbuf != NULL);
	
	node = bonobo_ui_container_get_tree (container, xml_path, FALSE, NULL);

	g_return_if_fail (node != NULL);

	bonobo_ui_util_xml_set_pixbuf (node, pixbuf);
	gdk_pixbuf_unref (pixbuf);

	parent_path = bonobo_ui_xml_get_parent_path (xml_path);
	bonobo_ui_component_set_tree (NULL, container, parent_path, node, NULL);

	xmlFreeNode (node);

	g_free (parent_path);
	g_free (path);
}

static void
update_pixmaps (Bonobo_UIContainer container)
{
	set_pixmap (container, "/Toolbar/MailGet", "fetch-mail.png");
	set_pixmap (container, "/Toolbar/MailCompose", "compose-message.png");
	set_pixmap (container, "/Toolbar/Reply", "reply.png");
	set_pixmap (container, "/Toolbar/ReplyAll", "reply-to-all.png");
	set_pixmap (container, "/Toolbar/Forward", "forward.png");
	set_pixmap (container, "/Toolbar/Move", "move-message.png");
	set_pixmap (container, "/Toolbar/Copy", "copy-message.png");
}

static void
control_activate (BonoboControl *control, BonoboUIHandler *uih,
		  FolderBrowser *fb)
{
	GtkWidget         *folder_browser;
	Bonobo_UIHandler   remote_uih;
	BonoboUIComponent *component;
	Bonobo_UIContainer container;

	remote_uih = bonobo_control_get_remote_ui_handler (control);
	bonobo_ui_handler_set_container (uih, remote_uih);
	bonobo_object_release_unref (remote_uih, NULL);

	container = bonobo_ui_compat_get_container (uih);
	g_return_if_fail (container != CORBA_OBJECT_NIL);
		
	folder_browser = bonobo_control_get_widget (control);

	component = bonobo_ui_compat_get_component (uih);
	bonobo_ui_component_add_verb_list_with_data (
		component, verbs, folder_browser);

	bonobo_ui_container_freeze (container, NULL);

	{ /* FIXME: sweeten this whole function */
		char *fname;
		xmlNode *ui;

		fname = bonobo_ui_util_get_ui_fname (
			EVOLUTION_DATADIR, "evolution-mail.xml");
		g_warning ("Attempting ui load from '%s'", fname);
		
		ui = bonobo_ui_util_new_ui (component, fname, "evolution-mail");
		
		bonobo_ui_component_set_tree (component, container, "/", ui, NULL);

		g_free (fname);
		xmlFreeNode (ui);
	}

	if (mail_config_thread_list ())
		bonobo_ui_container_set_prop (
			container, "/menu/View/Threaded", "state", "1", NULL);
	else
		bonobo_ui_container_set_prop (
			container, "/menu/View/Threaded", "state", "0", NULL);

	bonobo_ui_component_add_verb (
		component, "ViewThreaded",
		(BonoboUIVerbFn) message_list_toggle_threads,
		FOLDER_BROWSER (folder_browser)->message_list);

	create_ondemand_hooks (fb, uih);

	update_pixmaps (container);

	bonobo_ui_container_thaw (container, NULL);
}

static void
control_deactivate (BonoboControl *control,
		    BonoboUIHandler *uih,
		    FolderBrowser *fb)
{
	g_warning ("In mail control_deactivate");
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
	GtkWidget *folder_browser = user_data;

	control_list = g_list_remove (control_list, control);

	gtk_object_destroy (GTK_OBJECT (folder_browser));
}

BonoboControl *
folder_browser_factory_new_control (const char *uri,
				    const Evolution_Shell shell)
{
	BonoboControl *control;
	GtkWidget *folder_browser;

	folder_browser = folder_browser_new (shell);
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
