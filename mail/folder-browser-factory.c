/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * folder-browser-factory.c: A Bonobo Control factory for Folder Browsers
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-util.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>

#include "widgets/menus/gal-view-menus.h"

#include <gal/menus/gal-view-factory-etable.h>
#include <gal/menus/gal-view-etable.h>

#include "folder-browser-factory.h"

#include "folder-browser.h"
#include "mail.h"
#include "mail-callbacks.h"
#include "shell/Evolution.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail-session.h"

#include "evolution-shell-component-utils.h"
#include "camel/camel-vtrash-folder.h"

/* The FolderBrowser BonoboControls we have.  */
static EList *control_list = NULL;

/*
 * Add with 'folder_browser'
 */
BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("EditInvertSelection", invert_selection),
	BONOBO_UI_UNSAFE_VERB ("EditSelectAll", select_all),
/*	BONOBO_UI_UNSAFE_VERB ("EditSelectThread", select_thread),	*/
	BONOBO_UI_UNSAFE_VERB ("EmptyTrash", empty_trash),
	BONOBO_UI_UNSAFE_VERB ("FolderConfig", configure_folder),
	BONOBO_UI_UNSAFE_VERB ("FolderExpunge", expunge_folder),
	BONOBO_UI_UNSAFE_VERB ("ForgetPasswords", mail_session_forget_passwords),
	BONOBO_UI_UNSAFE_VERB ("MailCompose", compose_msg),
	BONOBO_UI_UNSAFE_VERB ("MailGetSend", send_receive_mail),
	BONOBO_UI_UNSAFE_VERB ("MailNext", next_msg),
	BONOBO_UI_UNSAFE_VERB ("MailNextUnread", next_unread_msg),
	BONOBO_UI_UNSAFE_VERB ("MailNextFlagged", next_flagged_msg),
	BONOBO_UI_UNSAFE_VERB ("MailPrevious", previous_msg),
	BONOBO_UI_UNSAFE_VERB ("MailPreviousUnread", previous_unread_msg),
	BONOBO_UI_UNSAFE_VERB ("MailPreviousFlagged", previous_flagged_msg),
	BONOBO_UI_UNSAFE_VERB ("MailStop", stop_threads),
	BONOBO_UI_UNSAFE_VERB ("MessageApplyFilters", apply_filters),
	BONOBO_UI_UNSAFE_VERB ("MessageCopy", copy_msg),
	BONOBO_UI_UNSAFE_VERB ("MessageDelete", delete_msg),
	BONOBO_UI_UNSAFE_VERB ("MessageForward", forward),
	BONOBO_UI_UNSAFE_VERB ("MessageForwardAttached", forward_attached),
	BONOBO_UI_UNSAFE_VERB ("MessageForwardInline", forward_inline),
	BONOBO_UI_UNSAFE_VERB ("MessageForwardQuoted", forward_quoted),
	BONOBO_UI_UNSAFE_VERB ("MessageMarkAllAsRead", mark_all_as_seen),
	BONOBO_UI_UNSAFE_VERB ("MessageMarkAsRead", mark_as_seen),
	BONOBO_UI_UNSAFE_VERB ("MessageMarkAsUnRead", mark_as_unseen),
	BONOBO_UI_UNSAFE_VERB ("MessageMove", move_msg),
	BONOBO_UI_UNSAFE_VERB ("MessageOpen", open_message),
	BONOBO_UI_UNSAFE_VERB ("MessageReplyAll", reply_to_all),
	BONOBO_UI_UNSAFE_VERB ("MessageReplySender", reply_to_sender),
	BONOBO_UI_UNSAFE_VERB ("MessageResend", resend_msg),
	BONOBO_UI_UNSAFE_VERB ("MessageSaveAs", save_msg),
	BONOBO_UI_UNSAFE_VERB ("MessageSearch", search_msg),
	BONOBO_UI_UNSAFE_VERB ("MessageUndelete", undelete_msg),
	BONOBO_UI_UNSAFE_VERB ("PrintMessage", print_msg),
	BONOBO_UI_UNSAFE_VERB ("PrintPreviewMessage", print_preview_msg),
	BONOBO_UI_UNSAFE_VERB ("ToolsFilterMailingList", filter_mlist),
	BONOBO_UI_UNSAFE_VERB ("ToolsFilterRecipient", filter_recipient),
	BONOBO_UI_UNSAFE_VERB ("ToolsFilters", filter_edit),
	BONOBO_UI_UNSAFE_VERB ("ToolsFilterSender", filter_sender),
	BONOBO_UI_UNSAFE_VERB ("ToolsFilterSubject", filter_subject),
	BONOBO_UI_UNSAFE_VERB ("ToolsSettings", providers_config),
	BONOBO_UI_UNSAFE_VERB ("ToolsSubscriptions", manage_subscriptions),
	BONOBO_UI_UNSAFE_VERB ("ToolsVFolderMailingList", vfolder_mlist),
	BONOBO_UI_UNSAFE_VERB ("ToolsVFolderRecipient", vfolder_recipient),
	BONOBO_UI_UNSAFE_VERB ("ToolsVFolders", vfolder_edit_vfolders),
	BONOBO_UI_UNSAFE_VERB ("ToolsVFolderSender", vfolder_sender),
	BONOBO_UI_UNSAFE_VERB ("ToolsVFolderSubject", vfolder_subject),
	BONOBO_UI_UNSAFE_VERB ("HideRead", hide_read),
	BONOBO_UI_UNSAFE_VERB ("HideSelected", hide_selected),
	BONOBO_UI_UNSAFE_VERB ("ViewShowAll", hide_none),
	BONOBO_UI_UNSAFE_VERB ("ViewLoadImages", load_images),
	
	BONOBO_UI_VERB_END
};

static EPixmap pixcache [] = {
	E_PIXMAP ("/commands/MailCompose", "new-message.xpm"),
	E_PIXMAP ("/commands/FolderConfig", "configure_16_folder.xpm"),
	E_PIXMAP ("/commands/PrintMessage", "print.xpm"),
	E_PIXMAP ("/commands/PrintPreviewMessage", "print-preview.xpm"),

	E_PIXMAP ("/commands/MessageDelete", "delete_message.xpm"),
	E_PIXMAP ("/commands/MessageUndelete", "undelete_message.xpm"),

	E_PIXMAP ("/commands/ViewHideRead", "hide_read_messages.xpm"),
	E_PIXMAP ("/commands/ViewHideSelected", "hide_selected_messages.xpm"),
	E_PIXMAP ("/commands/ViewShowAll", "show_all_messages.xpm"),

	E_PIXMAP ("/commands/MailGetSend", "send-receive.xpm"),
	E_PIXMAP ("/commands/MessageCopy", "copy_16_message.xpm"),
	E_PIXMAP ("/commands/MessageMove", "move_message.xpm"),
	E_PIXMAP ("/commands/MessageReplyAll", "reply_to_all.xpm"),
	E_PIXMAP ("/commands/MessageReplySender", "reply.xpm"),
	E_PIXMAP ("/commands/MessageForward", "forward.xpm"),

	E_PIXMAP ("/commands/ToolsSettings", "configure_16_mail.xpm"),
	
	E_PIXMAP ("/Toolbar/MailGetSend", "buttons/send-24-receive.png"),
	E_PIXMAP ("/Toolbar/MailCompose", "buttons/compose-message.png"),
	E_PIXMAP ("/Toolbar/MessageReplySender", "buttons/reply.png"),
	E_PIXMAP ("/Toolbar/MessageReplyAll", "buttons/reply-to-all.png"),
	E_PIXMAP ("/Toolbar/MessageForward", "buttons/forward.png"),
	E_PIXMAP ("/Toolbar/MessageMove", "buttons/move-message.png"),
	E_PIXMAP ("/Toolbar/MessageCopy", "buttons/copy-message.png"),

	E_PIXMAP_END
};

static void
display_view(GalViewCollection *collection,
	     GalView *view,
	     gpointer data)
{
	FolderBrowser *fb = data;
	if (GAL_IS_VIEW_ETABLE(view)) {
		e_tree_set_state_object(fb->message_list->tree, GAL_VIEW_ETABLE(view)->state);
	}
}

static void
folder_browser_setup_view_menus (FolderBrowser *fb,
				 BonoboUIComponent *uic)
{
	GalViewFactory *factory;
	ETableSpecification *spec;
	char *local_dir;

	g_assert (fb->view_collection == NULL);
	g_assert (fb->view_menus == NULL);

	fb->view_collection = gal_view_collection_new();

	local_dir = gnome_util_prepend_user_home ("/evolution/views/mail/");
	gal_view_collection_set_storage_directories(
		fb->view_collection,
		EVOLUTION_DATADIR "/evolution/views/mail/",
		local_dir);
	g_free (local_dir);

	spec = e_table_specification_new();
	e_table_specification_load_from_file(spec, EVOLUTION_ETSPECDIR "/message-list.etspec");

	factory = gal_view_factory_etable_new (spec);
	gtk_object_unref (GTK_OBJECT (spec));
	gal_view_collection_add_factory (fb->view_collection, factory);
	gtk_object_unref (GTK_OBJECT (factory));

	gal_view_collection_load(fb->view_collection);

	fb->view_menus = gal_view_menus_new(fb->view_collection);
	gal_view_menus_apply(fb->view_menus, uic, NULL);
	gtk_signal_connect(GTK_OBJECT(fb->view_collection), "display_view",
			   display_view, fb);
}

/* Gets rid of the view collection and view menus objects */
static void
folder_browser_discard_view_menus (FolderBrowser *fb)
{
	g_assert (fb->view_collection != NULL);
	g_assert (fb->view_menus != NULL);

	gtk_object_unref (GTK_OBJECT (fb->view_collection));
	fb->view_collection = NULL;

	gtk_object_unref (GTK_OBJECT (fb->view_menus));
	fb->view_menus = NULL;
}

static void
folder_browser_setup_property_menu (FolderBrowser *fb,
				    BonoboUIComponent *uic)
{
	char *name, *base = NULL;

	if (fb->uri)
		base = g_basename (fb->uri);

	if (base && base [0] != 0)
		name = g_strdup_printf (_("Properties for \"%s\""), base);
	else
		name = g_strdup (_("Properties"));

	bonobo_ui_component_set_prop (
		uic, "/menu/File/Folder/FolderConfig",
		"label", name, NULL);
	g_free (name);
}

/* Must be in the same order as MailConfigDisplayStyle */
char *message_display_styles[] = {
	"/commands/ViewNormal",
	"/commands/ViewFullHeaders",
	"/commands/ViewSource"
};

static void
control_activate (BonoboControl     *control,
		  BonoboUIComponent *uic,
		  FolderBrowser     *fb)
{
	GtkWidget         *folder_browser;
	Bonobo_UIContainer container;
	int state;

	container = bonobo_control_get_remote_ui_container (control);
	bonobo_ui_component_set_container (uic, container);
	bonobo_object_release_unref (container, NULL);

	g_assert (container == bonobo_ui_component_get_container (uic));
	g_return_if_fail (container != CORBA_OBJECT_NIL);
		
	folder_browser = bonobo_control_get_widget (control);

	bonobo_ui_component_add_verb_list_with_data (
		uic, verbs, folder_browser);

	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (
		uic, EVOLUTION_DATADIR,
		"evolution-mail.xml", "evolution-mail");

	state = mail_config_get_thread_list();
	bonobo_ui_component_set_prop(uic, "/commands/ViewThreaded", "state", state?"1":"0", NULL);
	bonobo_ui_component_add_listener(uic, "ViewThreaded", folder_browser_toggle_threads, folder_browser);
	/* FIXME: this kind of bypasses bonobo but seems the only way when we change components */
	folder_browser_toggle_threads(uic, "", Bonobo_UIComponent_STATE_CHANGED, state?"1":"0", folder_browser);

	state = mail_config_get_message_display_style ();
	bonobo_ui_component_set_prop (uic, message_display_styles[state],
				      "state", "1", NULL);
	bonobo_ui_component_add_listener (uic, "ViewNormal", folder_browser_set_message_display_style, folder_browser);
	bonobo_ui_component_add_listener (uic, "ViewFullHeaders", folder_browser_set_message_display_style, folder_browser);
	bonobo_ui_component_add_listener (uic, "ViewSource", folder_browser_set_message_display_style, folder_browser);
	/* FIXME: this kind of bypasses bonobo but seems the only way when we change components */
	folder_browser_set_message_display_style (uic, strrchr (message_display_styles[state], '/') + 1,
						  Bonobo_UIComponent_STATE_CHANGED, "1", folder_browser);

	if (fb->folder && CAMEL_IS_VTRASH_FOLDER(fb->folder)) {
		bonobo_ui_component_set_prop(uic, "/commands/HideDeleted", "sensitive", "0", NULL);
		state = FALSE;
	} else {
		state = mail_config_get_hide_deleted();
	}
	bonobo_ui_component_set_prop (uic, "/commands/HideDeleted", "state", state ? "1" : "0", NULL);
	bonobo_ui_component_add_listener (uic, "HideDeleted", folder_browser_toggle_hide_deleted,
					  folder_browser);
	/* FIXME: this kind of bypasses bonobo but seems the only way when we change components */
	folder_browser_toggle_hide_deleted (uic, "", Bonobo_UIComponent_STATE_CHANGED,
					    state ? "1" : "0", folder_browser);
	if (fb->folder && !folder_browser_is_sent (fb)) 
		bonobo_ui_component_set_prop (uic, "/commands/MessageResend", "sensitive", "0", NULL);
	
	folder_browser_setup_view_menus (fb, uic);
	folder_browser_setup_property_menu (fb, uic);
	
	e_pixmaps_update (uic, pixcache);

        bonobo_ui_component_set_prop(uic, "/commands/MailStop", "sensitive", "0", NULL);

	bonobo_ui_component_thaw (uic, NULL);

	if (fb->folder)
		mail_refresh_folder (fb->folder, NULL, NULL);
}

static void
control_deactivate (BonoboControl     *control,
		    BonoboUIComponent *uic,
		    FolderBrowser     *fb)
{
	bonobo_ui_component_rm (uic, "/", NULL);
 	bonobo_ui_component_unset_container (uic);

	if (fb->folder)
		mail_sync_folder (fb->folder, NULL, NULL);

	folder_browser_discard_view_menus (fb);
}

static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     gpointer user_data)
{
	BonoboUIComponent *uic;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	if (activate)
		control_activate (control, uic, user_data);
	else
		control_deactivate (control, uic, user_data);
}

static void
control_destroy_cb (BonoboControl *control,
		    GtkObject     *folder_browser)
{
	gtk_object_destroy (folder_browser);
}

static void
browser_destroy_cb (FolderBrowser *fb,
		    BonoboControl *control)
{
	EIterator *it;

	/* We do this from browser_destroy_cb rather than
	 * control_destroy_cb because currently, the controls
	 * don't seem to all get destroyed properly at quit
	 * time (but the widgets get destroyed by X). FIXME.
	 */

	for (it = e_list_get_iterator (control_list); e_iterator_is_valid (it); e_iterator_next (it)) {
		if (e_iterator_get (it) == control) {
			e_iterator_delete (it);
			break;
		}
	}
	gtk_object_unref (GTK_OBJECT (it));
}

BonoboControl *
folder_browser_factory_new_control (const char *uri,
				    const GNOME_Evolution_Shell shell)
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
	gtk_signal_connect (GTK_OBJECT (folder_browser), "destroy",
			    browser_destroy_cb, control);

	if (!control_list)
		control_list = e_list_new (NULL, NULL, NULL);

	e_list_append (control_list, control);

	return control;
}

EList *
folder_browser_factory_get_control_list (void)
{
	if (!control_list)
		control_list = e_list_new (NULL, NULL, NULL);
	return control_list;
}
