/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* addressbook-view.c
 *
 * Copyright (C) 2000, 2001, 2002, 2003 Ximian, Inc.
 * Copyright (C) 2004 Novell, Inc.
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
 * Author: Chris Toshok (toshok@ximian.com)
 */

#include <config.h>

#include <string.h>
#include <glib.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-href.h>
#include <libgnomeui/gnome-uidefs.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-property-bag.h>
#include <gal/util/e-util.h>

#include "widgets/misc/e-task-bar.h"
#include "widgets/misc/e-info-label.h"
#include "widgets/misc/e-source-selector.h"

#include "e-util/e-passwords.h"
#include "e-util/e-icon-factory.h"
#include "shell/e-user-creatable-items-handler.h"

#include "evolution-shell-component-utils.h"
#include "e-activity-handler.h"
#include "e-contact-editor.h"
#include "addressbook-config.h"
#include "addressbook.h"
#include "addressbook-view.h"
#include "addressbook-component.h"
#include "addressbook/gui/search/e-addressbook-search-dialog.h"
#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/gui/merging/eab-contact-merging.h"
#include "addressbook/printing/e-contact-print.h"
#include "addressbook/util/eab-book-util.h"

#include <libebook/e-book-async.h>

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

/* This is used for the addressbook status bar */
#define EVOLUTION_CONTACTS_PROGRESS_IMAGE "stock_contact"
static GdkPixbuf *progress_icon = NULL;

#define d(x)

#define PROPERTY_SOURCE_UID          "source_uid"
#define PROPERTY_FOLDER_URI          "folder_uri"

#define PROPERTY_SOURCE_UID_IDX      1
#define PROPERTY_FOLDER_URI_IDX      2

struct _AddressbookViewPrivate {
	GtkWidget *notebook;
	BonoboControl *folder_view_control;

	GtkWidget *statusbar_widget;
	EActivityHandler *activity_handler;

	GtkWidget *info_widget;
	GtkWidget *sidebar_widget;
	GtkWidget *selector;

	GConfClient *gconf_client;

	GHashTable *uid_to_view;
	EBook *book;
	guint activity_id;
	BonoboPropertyBag *properties;
	ESourceList *source_list;
	char *passwd;
	EUserCreatableItemsHandler *creatable_items_handler;
};

enum DndTargetType {
	DND_TARGET_TYPE_VCARD_LIST,
};
#define VCARD_TYPE "text/x-vcard"
static GtkTargetEntry drag_types[] = {
	{ VCARD_TYPE, 0, DND_TARGET_TYPE_VCARD_LIST }
};
static gint num_drag_types = sizeof(drag_types) / sizeof(drag_types[0]);

static void set_status_message (EABView *eav, const char *message, AddressbookView *view);
static void search_result (EABView *eav, EBookViewStatus status, AddressbookView *view);

static void addressbook_view_init	(AddressbookView      *view);
static void addressbook_view_class_init	(AddressbookViewClass *klass);
static void addressbook_view_dispose    (GObject *object);

static void set_prop (BonoboPropertyBag *bag, const BonoboArg   *arg, guint              arg_id,
		      CORBA_Environment *ev, gpointer           user_data);
static void get_prop (BonoboPropertyBag *bag, BonoboArg         *arg, guint              arg_id,
		      CORBA_Environment *ev, gpointer           user_data);

static EABView *
get_current_view (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;

	return EAB_VIEW (gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook),
						    gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook))));
}

static void
save_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_save_as(v);
}

static void
view_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_view(v);
}

static void
search_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		gtk_widget_show(eab_search_dialog_new(v));
}

static void
delete_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_delete_selection(v);
}

static void
print_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_print(v);
}

static void
print_preview_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_print_preview(v);
}

static void
stop_loading_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_stop(v);
}

static void
cut_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_cut(v);
}

static void
copy_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_copy(v);
}

static void
paste_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_paste(v);
}

static void
select_all_contacts_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_select_all (v);
}

static void
send_contact_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_send (v);
}

static void
send_contact_to_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_send_to (v);
}

static void
copy_contact_to_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_copy_to_folder (v);
}

static void
move_contact_to_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_move_to_folder (v);
}

static void
forget_passwords_cb (BonoboUIComponent *uih, void *user_data, const char *path)
{
	e_passwords_forget_passwords();
}

static void
set_status_message (EABView *eav, const char *message, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	EActivityHandler *activity_handler = priv->activity_handler;

	if (!message || !*message) {
		if (priv->activity_id != 0) {
			e_activity_handler_operation_finished (activity_handler, priv->activity_id);
			priv->activity_id = 0;
		}
	} else if (priv->activity_id == 0) {
		char *clientid = g_strdup_printf ("%p", view);

		if (progress_icon == NULL)
			progress_icon = e_icon_factory_get_icon (EVOLUTION_CONTACTS_PROGRESS_IMAGE, 16);

		priv->activity_id = e_activity_handler_operation_started (activity_handler, clientid,
									  progress_icon, message, TRUE);

		g_free (clientid);
	} else {
		e_activity_handler_operation_progressing (activity_handler, priv->activity_id, message, -1.0);
	}

}

static void
set_folder_bar_message (EABView *eav, const char *message, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	EABView *current_view = get_current_view (view);

	if (eav == current_view) {
		ESource *source = eav->source;

		if (source) {
			const char *name = e_source_peek_name (source);

			e_info_label_set_info((EInfoLabel*)priv->info_widget, name, message);
		}
	}
}

static void
search_result (EABView *eav, EBookViewStatus status, AddressbookView *view)
{
	eab_search_result_dialog (NULL /* XXX */, status);
}

static void
update_command_state (EABView *eav, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	BonoboUIComponent *uic;

	if (eav != get_current_view (view))
		return;

	g_object_ref (view);

	uic = bonobo_control_get_ui_component (priv->folder_view_control);

	if (bonobo_ui_component_get_container (uic) != CORBA_OBJECT_NIL) {
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSaveAsVCard",
					      "sensitive",
					      eab_view_can_save_as (eav) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsView",
					      "sensitive",
					      eab_view_can_view (eav) ? "1" : "0", NULL);

		/* Print Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPrint",
					      "sensitive",
					      eab_view_can_print (eav) ? "1" : "0", NULL);

		/* Print Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPrintPreview",
					      "sensitive",
					      eab_view_can_print (eav) ? "1" : "0", NULL);

		/* Delete Contact */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactDelete",
					      "sensitive",
					      eab_view_can_delete (eav) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsCut",
					      "sensitive",
					      eab_view_can_cut (eav) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsCopy",
					      "sensitive",
					      eab_view_can_copy (eav) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsPaste",
					      "sensitive",
					      eab_view_can_paste (eav) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSelectAll",
					      "sensitive",
					      eab_view_can_select_all (eav) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSendContactToOther",
					      "sensitive",
					      eab_view_can_send (eav) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsSendMessageToContact",
					      "sensitive",
					      eab_view_can_send_to (eav) ? "1" : "0", NULL);

		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsMoveToFolder",
					      "sensitive",
					      eab_view_can_move_to_folder (eav) ? "1" : "0", NULL);
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactsCopyToFolder",
					      "sensitive",
					      eab_view_can_copy_to_folder (eav) ? "1" : "0", NULL);

		/* Stop */
		bonobo_ui_component_set_prop (uic,
					      "/commands/ContactStop",
					      "sensitive",
					      eab_view_can_stop (eav) ? "1" : "0", NULL);
	}

	g_object_unref (view);
}

static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ContactsPrint", print_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsPrintPreview", print_preview_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSaveAsVCard", save_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsView", view_contact_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactDelete", delete_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactStop", stop_loading_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactsCut", cut_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsCopy", copy_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsPaste", paste_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSelectAll", select_all_contacts_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactsSendContactToOther", send_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSendMessageToContact", send_contact_to_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsMoveToFolder", move_contact_to_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsCopyToFolder", copy_contact_to_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsForgetPasswords", forget_passwords_cb),
	/* ContactsViewPreview is a toggle */

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/menu/File/FileOps/ContactsSaveAsVCard", "stock_save_as", 16),
	E_PIXMAP ("/menu/File/Print/ContactsPrint", "stock_print", 16),
	E_PIXMAP ("/menu/File/Print/ContactsPrintPreview", "stock_print-preview", 16),

	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactsCut", "stock_cut", 16),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactsCopy", "stock_copy", 16),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactsPaste", "stock_paste", 16),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/ContactDelete", "stock_delete", 16),

	E_PIXMAP ("/Toolbar/ContactsPrint", "stock_print", 24),
	E_PIXMAP ("/Toolbar/ContactDelete", "stock_delete", 24),

	E_PIXMAP_END
};

static void
control_activate (BonoboControl     *control,
		  BonoboUIComponent *uic,
		  AddressbookView   *view)
{
	AddressbookViewPrivate *priv = view->priv;
	Bonobo_UIContainer remote_ui_container;
	EABView *v = get_current_view (view);

	remote_ui_container = bonobo_control_get_remote_ui_container (control, NULL);
	bonobo_ui_component_set_container (uic, remote_ui_container, NULL);
	bonobo_object_release_unref (remote_ui_container, NULL);

	bonobo_ui_component_add_verb_list_with_data (
		uic, verbs, view);
	
	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (uic, PREFIX,
			       EVOLUTION_UIDIR "/evolution-addressbook.xml",
			       "evolution-addressbook", NULL);

	if (v)
		eab_view_setup_menus (v, uic);

	e_pixmaps_update (uic, pixmaps);

	e_user_creatable_items_handler_activate (priv->creatable_items_handler, uic);

	bonobo_ui_component_thaw (uic, NULL);

	if (v)
		update_command_state (v, view);
}

static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     AddressbookView *view)
{
	BonoboUIComponent *uic;
	EABView *v = get_current_view (view);

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	if (activate) {
		control_activate (control, uic, view);
		if (activate && v && v->model)
			eab_model_force_folder_bar_message (v->model);
	} else {
		bonobo_ui_component_unset_container (uic, NULL);
		eab_view_discard_menus (v);
	}
}

static void
gather_uids_foreach (char *key,
		     gpointer value,
		     GList **list)
{
	(*list) = g_list_prepend (*list, key);
}

static void
source_list_changed_cb (ESourceList *source_list, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	GList *uids, *l;
	EABView *v;

	uids = NULL;
	g_hash_table_foreach (priv->uid_to_view, (GHFunc)gather_uids_foreach, &uids);

	for (l = uids; l; l = l->next) {
		char *uid = l->data;
		if (e_source_list_peek_source_by_uid (source_list, uid)) {
			/* the source still exists, do nothing */
		}
		else {
			/* the source no longer exists, remove the
			   view and remove it from our hash table. */
			v = g_hash_table_lookup (priv->uid_to_view,
						 uid);
			g_hash_table_remove (priv->uid_to_view, uid);
			gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook),
						  gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
									 GTK_WIDGET (v)));
			g_object_unref (v);
		}
	}

	/* make sure we've got the current view selected and updated
	   properly */
	v = get_current_view (view);
	if (v) {
		eab_view_setup_menus (v, bonobo_control_get_ui_component (priv->folder_view_control));
		update_command_state (v, view);
	}
}

static void
load_uri_for_selection (ESourceSelector *selector,
			BonoboControl *view_control)
{
	ESource *selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (selector));

	if (selected_source != NULL) {
		bonobo_control_set_property (view_control, NULL, "source_uid", TC_CORBA_string,
					     e_source_peek_uid (selected_source), NULL);
	}
}

static ESource *
find_first_source (ESourceList *source_list)
{
	GSList *groups, *sources, *l, *m;
			
	groups = e_source_list_peek_groups (source_list);
	for (l = groups; l; l = l->next) {
		ESourceGroup *group = l->data;
				
		sources = e_source_group_peek_sources (group);
		for (m = sources; m; m = m->next) {
			ESource *source = m->data;

			return source;
		}				
	}

	return NULL;
}

static void
save_primary_selection (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *source;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	if (!source)
		return;

	/* Save the selection for next time we start up */
	gconf_client_set_string (priv->gconf_client,
				 "/apps/evolution/addressbook/display/primary_addressbook",
				 e_source_peek_uid (source), NULL);
}

static ESource *
get_primary_source (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *source;
	char *uid;

	uid = gconf_client_get_string (priv->gconf_client,
				       "/apps/evolution/addressbook/display/primary_addressbook",
				       NULL);
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		g_free (uid);
	} else {
		/* Try to create a default if there isn't one */
		source = find_first_source (priv->source_list);
	}

	return source;
}

static void
load_primary_selection (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *source;

	source = get_primary_source (view);
	if (source)
		e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (priv->selector), source);
}

/* Folder popup menu callbacks */

static void
add_popup_menu_item (GtkMenu *menu, const char *label, const char *pixmap,
		     GCallback callback, gpointer user_data, gboolean sensitive)
{
	GtkWidget *item, *image;

	if (pixmap) {
		item = gtk_image_menu_item_new_with_label (label);

		/* load the image */
		if (g_file_test (pixmap, G_FILE_TEST_EXISTS))
			image = gtk_image_new_from_file (pixmap);
		else
			image = gtk_image_new_from_stock (pixmap, GTK_ICON_SIZE_MENU);

		if (image) {
			gtk_widget_show (image);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		}
	} else {
		item = gtk_menu_item_new_with_label (label);
	}

	if (callback)
		g_signal_connect (G_OBJECT (item), "activate", callback, user_data);

	if (!sensitive)
		gtk_widget_set_sensitive (item, FALSE);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

static void
delete_addressbook_cb (GtkWidget *widget, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *selected_source;
	GtkWidget *dialog;
	EBook  *book;
	gboolean removed = FALSE;
	GError *error = NULL;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	if (!selected_source)
		return;

	/* Create the confirmation dialog */
	dialog = gtk_message_dialog_new (
		GTK_WINDOW (gtk_widget_get_toplevel (widget)),
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_YES_NO,
		_("Address book '%s' will be removed. Are you sure you want to continue?"),
		e_source_peek_name (selected_source));
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_YES) {
		gtk_widget_destroy (dialog);
		return;
	}

	/* Remove local data */
	book = e_book_new ();
	if (e_book_load_source (book, selected_source, TRUE, &error))
		removed = e_book_remove (book, &error);

	if (removed) {
		/* Remove source */
		if (e_source_selector_source_is_selected (E_SOURCE_SELECTOR (priv->selector),
							  selected_source))
			e_source_selector_unselect_source (E_SOURCE_SELECTOR (priv->selector),
							   selected_source);
		
		e_source_group_remove_source (e_source_peek_group (selected_source), selected_source);

		e_source_list_sync (priv->source_list, NULL);
	} else {
		GtkWidget *error_dialog;

		error_dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (widget)),
						       GTK_DIALOG_MODAL,
						       GTK_MESSAGE_ERROR,
						       GTK_BUTTONS_CLOSE,
						       "Error removing address book: %s",
						       error->message);
		gtk_dialog_run (GTK_DIALOG (error_dialog));
		gtk_widget_destroy (error_dialog);

		g_error_free (error);
	}

	g_object_unref (book);
	gtk_widget_destroy (dialog);
}

static void
new_addressbook_cb (GtkWidget *widget, AddressbookView *view)
{
	addressbook_config_create_new_source (gtk_widget_get_toplevel (widget));
}

static void
edit_addressbook_cb (GtkWidget *widget, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *selected_source;

	selected_source =
		e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	if (!selected_source)
		return;

	addressbook_config_edit_source (gtk_widget_get_toplevel (widget), selected_source);
}

/* Callbacks.  */

static void
primary_source_selection_changed_callback (ESourceSelector *selector,
					   AddressbookView *view)
{
	load_uri_for_selection (selector, view->priv->folder_view_control);
	save_primary_selection (view);
}


static void
fill_popup_menu_callback (ESourceSelector *selector, GtkMenu *menu, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	gboolean sensitive;
	gboolean local_addressbook;
	ESource *selected_source;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	sensitive = selected_source ? TRUE : FALSE;

	local_addressbook =  (!strcmp ("system", e_source_peek_relative_uri (selected_source)));
		
	add_popup_menu_item (menu, _("New Address Book"), NULL, G_CALLBACK (new_addressbook_cb), view, TRUE);
	add_popup_menu_item (menu, _("Delete"), GTK_STOCK_DELETE, G_CALLBACK (delete_addressbook_cb), view, sensitive && !local_addressbook);
	add_popup_menu_item (menu, _("Properties..."), NULL, G_CALLBACK (edit_addressbook_cb), view, sensitive);
}

static gboolean
selector_tree_drag_drop (GtkWidget *widget, 
			 GdkDragContext *context, 
			 int x, 
			 int y, 
			 guint time, 
			 AddressbookView *view)
{
	GtkTreeViewColumn *column;
	int cell_x;
	int cell_y;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer data;
	
	if (!gtk_tree_view_get_path_at_pos  (GTK_TREE_VIEW (widget), x, y, &path, &column, &cell_x, &cell_y))
		return FALSE;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	gtk_tree_model_get (model, &iter, 0, &data, -1);
	
	if (E_IS_SOURCE_GROUP (data)) {
		g_object_unref (data);
		gtk_tree_path_free (path);
		return FALSE;
	}
	
	gtk_tree_path_free (path);
	return TRUE;
}
	
static gboolean
selector_tree_drag_motion (GtkWidget *widget,
			   GdkDragContext *context,
			   int x,
			   int y)
{
	GtkTreePath *path = NULL;
	gpointer data = NULL;
	GtkTreeViewDropPosition pos;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GdkDragAction action;
	
	if (!gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
						x, y, &path, &pos))
		goto finish;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	
	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto finish;
	
	gtk_tree_model_get (model, &iter, 0, &data, -1);

	if (E_IS_SOURCE_GROUP (data) || e_source_get_readonly (data))
		goto finish;
	
	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW (widget), path, GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
	action = context->suggested_action;

 finish:
	if (path)
		gtk_tree_path_free (path);
	if (data)
		g_object_unref (data);
      
	gdk_drag_status (context, action, GDK_CURRENT_TIME);
	return TRUE;
}

static gboolean 
selector_tree_drag_data_received (GtkWidget *widget, 
				  GdkDragContext *context, 
				  gint x, 
				  gint y, 
				  GtkSelectionData *data,
				  guint info,
				  guint time,
				  gpointer user_data)
{
	GtkTreePath *path = NULL;
	GtkTreeViewDropPosition pos;
	gpointer source = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean success = FALSE;

	EBook *book;
	GList *contactlist;
	GList *l;

	if (!gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
						x, y, &path, &pos))
		goto finish;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	
	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto finish;
	
	gtk_tree_model_get (model, &iter, 0, &source, -1);

	if (E_IS_SOURCE_GROUP (source) || e_source_get_readonly (source))
		goto finish;
	
	book = e_book_new ();
	if (!book) {
		g_message (G_STRLOC ":Couldn't create EBook.");
		return FALSE;
	}
	e_book_load_source (book, source, TRUE, NULL);
	contactlist = eab_contact_list_from_string (data->data);
	
	for (l = contactlist; l; l = l->next) {
		EContact *contact = l->data;
		
		/* XXX NULL for a callback /sigh */
		if (contact)
			eab_merging_book_add_contact (book, contact, NULL /* XXX */, NULL);
		success = TRUE;
	}
	
	g_list_foreach (contactlist, (GFunc)g_object_unref, NULL);
	g_list_free (contactlist);
	g_object_unref (book);

 finish:
	if (path)
		gtk_tree_path_free (path);
	if (source)
		g_object_unref (source);
		       
	gtk_drag_finish (context, success, context->action == GDK_ACTION_MOVE, time);

	return TRUE;
}	

static void
selector_tree_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, gpointer data)
{
	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW (widget), NULL, GTK_TREE_VIEW_DROP_BEFORE);
}


static void
destroy_callback(gpointer data, GObject *where_object_was)
{
	AddressbookView *view = data;
	g_object_unref (view);
}

GType
addressbook_view_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (AddressbookViewClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) addressbook_view_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EABView),
			0,             /* n_preallocs */
			(GInstanceInitFunc) addressbook_view_init,
		};

		type = g_type_register_static (PARENT_TYPE, "AddressbookView", &info, 0);
	}

	return type;
}

static void
addressbook_view_class_init (AddressbookViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = addressbook_view_dispose;

	parent_class = g_type_class_peek_parent (klass);
}

static void
addressbook_view_init (AddressbookView *view)
{
	AddressbookViewPrivate *priv;
	GtkWidget *selector_scrolled_window;

	view->priv = 
		priv = g_new0 (AddressbookViewPrivate, 1);

	priv->gconf_client = addressbook_component_peek_gconf_client (addressbook_component_peek ());

	priv->uid_to_view = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify)g_free, NULL);

	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);

	g_object_weak_ref (G_OBJECT (priv->notebook), destroy_callback, view);

	/* Create the control. */
	priv->folder_view_control = bonobo_control_new (priv->notebook);

	gtk_widget_show (priv->notebook);

	priv->properties = bonobo_property_bag_new (get_prop, set_prop, view);

	bonobo_property_bag_add (priv->properties,
				 PROPERTY_SOURCE_UID, PROPERTY_SOURCE_UID_IDX,
				 BONOBO_ARG_STRING, NULL,
				 _("UID of the contacts source that the view will display"), 0);

	bonobo_property_bag_add (priv->properties,
				 PROPERTY_FOLDER_URI, PROPERTY_FOLDER_URI_IDX,
				 BONOBO_ARG_STRING, NULL,
				 _("The URI that the address book will display"), 0);

	bonobo_control_set_properties (priv->folder_view_control,
				       bonobo_object_corba_objref (BONOBO_OBJECT (priv->properties)),
				       NULL);

	e_book_get_addressbooks (&priv->source_list, NULL);
	g_signal_connect (priv->source_list,
			  "changed",
			  G_CALLBACK (source_list_changed_cb), view);

	priv->creatable_items_handler = e_user_creatable_items_handler_new ("contacts", NULL, NULL);

	g_signal_connect (priv->folder_view_control, "activate",
			  G_CALLBACK (control_activate_cb), view);

	priv->activity_handler = e_activity_handler_new ();

	priv->statusbar_widget = e_task_bar_new ();
	gtk_widget_show (priv->statusbar_widget);

	e_activity_handler_attach_task_bar (priv->activity_handler,
					    E_TASK_BAR (priv->statusbar_widget));

	priv->info_widget = e_info_label_new("stock_contact");
	e_info_label_set_info((EInfoLabel*)priv->info_widget, _("Contacts"), "");
	gtk_widget_show (priv->info_widget);

	priv->selector = e_source_selector_new (priv->source_list);

	g_signal_connect  (priv->selector, "drag-motion", G_CALLBACK (selector_tree_drag_motion), view);
	g_signal_connect  (priv->selector, "drag-leave", G_CALLBACK (selector_tree_drag_leave), view);
	g_signal_connect  (priv->selector, "drag-drop", G_CALLBACK (selector_tree_drag_drop), view);
	g_signal_connect  (priv->selector, "drag-data-received", G_CALLBACK (selector_tree_drag_data_received), view);
	gtk_drag_dest_set (priv->selector, GTK_DEST_DEFAULT_ALL, drag_types, num_drag_types, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	e_source_selector_show_selection (E_SOURCE_SELECTOR (priv->selector), FALSE);
	gtk_widget_show (priv->selector);

	selector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (selector_scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (selector_scrolled_window), priv->selector);
	gtk_widget_show (selector_scrolled_window);

	priv->sidebar_widget = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (priv->sidebar_widget), priv->info_widget, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (priv->sidebar_widget), selector_scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (priv->sidebar_widget);

	g_signal_connect_object (priv->selector, "primary_selection_changed",
				 G_CALLBACK (primary_source_selection_changed_callback),
				 G_OBJECT (view), 0);
	g_signal_connect_object (priv->selector, "fill_popup_menu",
				 G_CALLBACK (fill_popup_menu_callback),
				 G_OBJECT (view), 0);

	load_primary_selection (view);
	load_uri_for_selection (E_SOURCE_SELECTOR (priv->selector), priv->folder_view_control);
}

static void
addressbook_view_dispose (GObject *object)
{
	AddressbookView *view = ADDRESSBOOK_VIEW (object);
	AddressbookViewPrivate *priv = view->priv;

	if (view->priv) {
		if (priv->book)
			g_object_unref (priv->book);
	
		if (priv->properties)
			bonobo_object_unref (BONOBO_OBJECT(priv->properties));
		
		g_free(priv->passwd);

		if (priv->source_list)
			g_object_unref (priv->source_list);

		if (priv->uid_to_view)
			g_hash_table_destroy (priv->uid_to_view);

		if (priv->creatable_items_handler)
			g_object_unref (priv->creatable_items_handler);

		g_free (view->priv);
		view->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

typedef struct {
	EABView *view;
	ESource *source;
} BookOpenData;

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	BookOpenData *data = closure;
	EABView *view = data->view;
	ESource *source = data->source;

	g_free (data);

	/* we always set the "source" property on the EABView, since
	   we use it to reload a previously failed book. */
	g_object_set(view,
		     "source", source,
		     NULL);

	if (status == E_BOOK_ERROR_OK) {
		g_object_set(view,
			     "book", book,
			     NULL);

		if (view->model)
			eab_model_force_folder_bar_message (view->model);
	}
	else {
		eab_load_error_dialog (NULL /* XXX */, source, status);
	}


	g_object_unref (source);
}

static void
activate_source (AddressbookView *view,
		 ESource *source,
		 const char *uid)
{
	AddressbookViewPrivate *priv = view->priv;
	GtkWidget *uid_view;
	EBook *book;
	BookOpenData *data;

	uid_view = g_hash_table_lookup (priv->uid_to_view, uid);

	if (uid_view) {
		/* there is a view for this uid.  make
		   sure that the view actually
		   contains an EBook (if it doesn't
		   contain an EBook a previous load
		   failed.  try to load it again */
		g_object_get (uid_view,
			      "book", &book,
			      NULL);

		if (book) {
			g_object_unref (book);
		}
		else {
			book = e_book_new ();

			g_object_get (uid_view,
				      "source", &source,
				      NULL);

			/* source can be NULL here, if
			   a previous load hasn't
			   actually made it to
			   book_open_cb yet. */
			if (source) {
				data = g_new (BookOpenData, 1);
				data->view = g_object_ref (uid_view);
				data->source = source; /* transfer the ref we get back from g_object_get */

				addressbook_load_source (book, source, book_open_cb, data);
			}
		}
	}
	else {
		/* we don't have a view for this uid already
		   set up. */
		GtkWidget *label = gtk_label_new (uid);

		uid_view = eab_view_new ();

		gtk_widget_show (uid_view);
		gtk_widget_show (label);

		g_object_set (uid_view, "type", EAB_VIEW_TABLE, NULL);

		gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
					  uid_view,
					  label);

		g_hash_table_insert (priv->uid_to_view, g_strdup (uid), uid_view);

		g_signal_connect (uid_view, "status_message",
				  G_CALLBACK(set_status_message), view);

		g_signal_connect (uid_view, "search_result",
				  G_CALLBACK(search_result), view);

		g_signal_connect (uid_view, "folder_bar_message",
				  G_CALLBACK(set_folder_bar_message), view);

		g_signal_connect (uid_view, "command_state_change",
				  G_CALLBACK(update_command_state), view);

		book = e_book_new ();

		data = g_new (BookOpenData, 1);
		data->view = g_object_ref (uid_view);
		data->source = g_object_ref (source);

		addressbook_load_source (book, source, book_open_cb, data);
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
				       gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
							      uid_view));

	if (EAB_VIEW (uid_view)->model)
		eab_model_force_folder_bar_message (EAB_VIEW (uid_view)->model);

	/* change menus/toolbars to reflect the new view, assuming we are already displayed */
	if (bonobo_ui_component_get_container (bonobo_control_get_ui_component (priv->folder_view_control)) != CORBA_OBJECT_NIL) {
		eab_view_setup_menus (EAB_VIEW (uid_view), bonobo_control_get_ui_component (priv->folder_view_control));
		update_command_state (EAB_VIEW (uid_view), view);
	}
}

static void
set_prop (BonoboPropertyBag *bag,
	  const BonoboArg   *arg,
	  guint              arg_id,
	  CORBA_Environment *ev,
	  gpointer           user_data)
{
	AddressbookView *view = user_data;
	AddressbookViewPrivate *priv = view->priv;
	ESource *source;

	switch (arg_id) {

	case PROPERTY_FOLDER_URI_IDX: {
		const gchar *string = BONOBO_ARG_GET_STRING (arg);
		ESourceGroup *group;

		group = e_source_group_new ("", string);
		source = e_source_new ("", "");
		e_source_set_group (source, group);

		/* we use the uri as the uid here. */
		activate_source (view, source, string);

		g_object_unref (group);

		break;
	}
	case PROPERTY_SOURCE_UID_IDX: {
		const gchar *uid;

		uid = BONOBO_ARG_GET_STRING (arg);

		source = e_source_list_peek_source_by_uid (priv->source_list, uid);

		if (source) {
			activate_source (view, source, uid);
		}
		else {
			g_warning ("Could not find source by UID '%s'!", uid);
		}

		break;
	}
	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}

static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg         *arg,
	  guint              arg_id,
	  CORBA_Environment *ev,
	  gpointer           user_data)
{
	AddressbookView *view = user_data;
	EABView *v = get_current_view (view);
	ESource *source = NULL;

	switch (arg_id) {

	case PROPERTY_FOLDER_URI_IDX:
		if (v) {
			g_object_get (v,
				      "source", &source,
				      NULL);
		}

		if (source) {
			char *uri = e_source_get_uri (source);

			BONOBO_ARG_SET_STRING (arg, uri);

			g_free (uri);
			g_object_unref (source);
		}
		else {
			BONOBO_ARG_SET_STRING (arg, "");
		}
		break;

	case PROPERTY_SOURCE_UID_IDX:
		if (v) {
			g_object_get (v,
				      "source", &source,
				      NULL);
		}

		if (source) {
			BONOBO_ARG_SET_STRING (arg, e_source_peek_uid (source));

			g_object_unref (source);
		}
		else {
			BONOBO_ARG_SET_STRING (arg, "");
		}

		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
	}
}

AddressbookView *
addressbook_view_new (void)
{
	return g_object_new (ADDRESSBOOK_TYPE_VIEW, NULL);
}

EActivityHandler*
addressbook_view_peek_activity_handler (AddressbookView *view)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_VIEW (view), NULL);

	return view->priv->activity_handler;
}

GtkWidget*
addressbook_view_peek_info_label (AddressbookView *view)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_VIEW (view), NULL);

	return view->priv->info_widget;
}

GtkWidget*
addressbook_view_peek_sidebar (AddressbookView *view)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_VIEW (view), NULL);

	return view->priv->sidebar_widget;
}

GtkWidget*
addressbook_view_peek_statusbar (AddressbookView *view)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_VIEW (view), NULL);

	return view->priv->statusbar_widget;
}

BonoboControl*
addressbook_view_peek_folder_view (AddressbookView *view)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_VIEW (view), NULL);

	return view->priv->folder_view_control;
}
