/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Shakti Sen <shprasad@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <e-folder-exchange.h>
#include <exchange-hierarchy.h>
#include <calendar/gui/e-cal-popup.h>
#include <mail/em-popup.h>
#include <mail/em-menu.h>
#include <libedataserverui/e-source-selector.h>
#include <e-util/e-error.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <mail/mail-mt.h>
#include <mail/mail-ops.h>

#include "exchange-operations.h"
#include "addressbook/gui/widgets/eab-popup.h"
#include "exchange-folder-subscription.h"

void org_gnome_exchange_folder_subscription (EPlugin *ep, EMMenuTargetSelect *target, const gchar *fname);
void org_gnome_exchange_inbox_subscription (EPlugin *ep, EMMenuTargetSelect *target);
void org_gnome_exchange_addressbook_subscription (EPlugin *ep, EMMenuTargetSelect *target);
void org_gnome_exchange_calendar_subscription (EPlugin *ep, EMMenuTargetSelect *target);
void org_gnome_exchange_tasks_subscription (EPlugin *ep, EMMenuTargetSelect *target);
void org_gnome_exchange_check_subscribed (EPlugin *ep, ECalPopupTargetSource *target);
void org_gnome_exchange_folder_unsubscribe (EPopup *ep, EPopupItem *p, gpointer data);
void org_gnome_exchange_check_address_book_subscribed (EPlugin *ep, EABPopupTargetSource *target);
void org_gnome_exchange_folder_ab_unsubscribe (EPopup *ep, EPopupItem *p, gpointer data);
void org_gnome_exchange_check_inbox_subscribed (EPlugin *ep, EMPopupTargetFolder *target);
void org_gnome_exchange_folder_inbox_unsubscribe (EPopup *ep, EPopupItem *p, gpointer data);
void popup_free (EPopup *ep, GSList *items, gpointer data);
void popup_inbox_free (EPopup *ep, GSList *items, gpointer data);
void popup_ab_free (EPopup *ep, GSList *items, gpointer data);
static void exchange_get_folder (gchar *uri, CamelFolder *folder, gpointer data);

#define CONF_KEY_SELECTED_CAL_SOURCES "/apps/evolution/calendar/display/selected_calendars"

static EPopupItem popup_inbox_items[] = {
	{ E_POPUP_ITEM, (gchar *) "29.inbox_unsubscribe", (gchar *) N_("Unsubscribe Folder..."), org_gnome_exchange_folder_inbox_unsubscribe, NULL, (gchar *) "folder-new", 0, EM_POPUP_FOLDER_INFERIORS }
};

void
popup_inbox_free (EPopup *ep, GSList *items, gpointer data)
{
	g_slist_free (items);
}

void
org_gnome_exchange_folder_inbox_unsubscribe (EPopup *ep, EPopupItem *p, gpointer data)
{
	ExchangeAccount *account = NULL;
	EMPopupTargetFolder *target = data;
	gchar *path = NULL;
	gchar *stored_path = NULL;
	const gchar *inbox_uri = NULL;
	const gchar *inbox_physical_uri = NULL;
	gchar *target_uri = NULL;
	EFolder *inbox;
	ExchangeAccountFolderResult result;

	account = exchange_operations_get_exchange_account ();

	if (!account)
		return;

	target_uri = g_strdup (target->uri);
	path = target->uri + strlen ("exchange://") + strlen (account->account_filename);
	/* User will be able to unsubscribe by doing a right click on
	   any one of this two-<other user's>Inbox or the
	   <other user's folder> tree.
	  */
	stored_path = strrchr (path + 1, '/');

	if (stored_path)
		path[stored_path - path] = '\0';

	result = exchange_account_remove_shared_folder (account, path);
	switch (result) {
		case EXCHANGE_ACCOUNT_FOLDER_OK:
			break;
		case EXCHANGE_ACCOUNT_FOLDER_ALREADY_EXISTS:
			e_error_run (NULL, ERROR_DOMAIN ":folder-exists-error", NULL);
			return;
		case EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST:
			e_error_run (NULL, ERROR_DOMAIN ":folder-doesnt-exist-error", NULL);
			return;
		case EXCHANGE_ACCOUNT_FOLDER_UNKNOWN_TYPE:
			e_error_run (NULL, ERROR_DOMAIN ":folder-unknown-type", NULL);
			return;
		case EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED:
			e_error_run (NULL, ERROR_DOMAIN ":folder-perm-error", NULL);
			return;
		case EXCHANGE_ACCOUNT_FOLDER_OFFLINE:
			e_error_run (NULL, ERROR_DOMAIN ":folder-offline-error", NULL);
			return;
		case EXCHANGE_ACCOUNT_FOLDER_UNSUPPORTED_OPERATION:
			e_error_run (NULL, ERROR_DOMAIN ":folder-unsupported-error", NULL);
			return;
		case EXCHANGE_ACCOUNT_FOLDER_GENERIC_ERROR:
			e_error_run (NULL, ERROR_DOMAIN ":folder-generic-error", NULL);
			return;
		case EXCHANGE_ACCOUNT_FOLDER_GC_NOTREACHABLE:
			e_error_run (NULL, ERROR_DOMAIN ":folder-no-gc-error", NULL);
			return;
		case EXCHANGE_ACCOUNT_FOLDER_NO_SUCH_USER:
			e_error_run (NULL, ERROR_DOMAIN ":no-user-error", NULL);
			return;
	}

	/* We need to get the physical uri for the Inbox */
	inbox_uri = exchange_account_get_standard_uri (account, "inbox");
	inbox = exchange_account_get_folder (account, inbox_uri);
	inbox_physical_uri = e_folder_get_physical_uri (inbox);

	/* To get the CamelStore/Folder */
	mail_get_folder (inbox_physical_uri, 0, exchange_get_folder, target_uri, mail_msg_unordered_push);

}

static CamelFolderInfo *
ex_create_folder_info (CamelStore *store, gchar *name, gchar *uri,
                  gint unread_count, gint flags)
{
        CamelFolderInfo *info;
        const gchar *path;

        path = strstr (uri, "://");
        if (!path)
                return NULL;
        path = strchr (path + 3, '/');
        if (!path)
                return NULL;

        info = camel_folder_info_new ();
        info->name = name;
        info->uri = uri;
        info->full_name = g_strdup (path + 1);
        info->unread = unread_count;

        return info;
}

static void
exchange_get_folder (gchar *uri, CamelFolder *folder, gpointer data)
{
	CamelStore *store;
	CamelException ex;
	CamelFolderInfo *info;
	gchar *name = NULL;
	gchar *stored_name = NULL;
	gchar *target_uri = (gchar *)data;
	ExchangeAccount *account = NULL;

	account = exchange_operations_get_exchange_account ();

	if (!account)
		return;

	/* Get the subscribed folder name. */
	name = target_uri + strlen ("exchange://") + strlen (account->account_filename);
	stored_name = strrchr (name + 1, '/');

	if (stored_name)
		name[stored_name - name] = '\0';

	camel_exception_init (&ex);
	store = camel_folder_get_parent_store (folder);

	/* Construct the CamelFolderInfo */
	info = ex_create_folder_info (store, name, target_uri, -1, 0);
	camel_object_trigger_event (CAMEL_OBJECT (store),
				    "folder_unsubscribed", info);
	g_free (target_uri);
}

void
org_gnome_exchange_check_inbox_subscribed (EPlugin *ep, EMPopupTargetFolder *target)
{
	GSList *menus = NULL;
	gint i = 0;
	ExchangeAccount *account = NULL;
	gchar *path = NULL;
	gchar *sub_folder = NULL;

	if (!g_strrstr (target->uri, "exchange://"))
		return;

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;

	if (strlen (target->uri) <= strlen ("exchange://") + strlen (account->account_filename))
		return;

	path = g_strdup (target->uri + strlen ("exchange://") + strlen (account->account_filename));
	sub_folder = strchr (path, '@');

	if (!sub_folder || !g_strrstr(sub_folder, "/")) {
		g_free (path);
		return;
	}

	g_free (path);


        for (i = 0; i < sizeof (popup_inbox_items) / sizeof (popup_inbox_items[0]); i++)
                menus = g_slist_prepend (menus, &popup_inbox_items[i]);

        e_popup_add_items (target->target.popup, menus, NULL, popup_inbox_free, target);
}

static EPopupItem popup_items[] = {
	{ E_POPUP_ITEM, (gchar *) "29.calendar_unsubscribe", (gchar *) N_("Unsubscribe Folder..."), org_gnome_exchange_folder_unsubscribe, NULL, (gchar *) "folder-new", 0, EM_POPUP_FOLDER_INFERIORS }
};

void
popup_free (EPopup *ep, GSList *items, gpointer data)
{
	g_slist_free (items);
}

static EPopupItem popup_ab_items[] = {
	{ E_POPUP_ITEM, (gchar *) "29.address_book_unsubscribe", (gchar *) N_("Unsubscribe Folder..."), org_gnome_exchange_folder_ab_unsubscribe, NULL, (gchar *) "folder-new", 0, EM_POPUP_FOLDER_INFERIORS }
};

void
popup_ab_free (EPopup *ep, GSList *items, gpointer data)
{
	g_slist_free (items);
}

void
org_gnome_exchange_check_address_book_subscribed (EPlugin *ep, EABPopupTargetSource *target)
{
	GSList *menus = NULL;
	gint i = 0;
	ESource *source = NULL;
	gchar *uri = NULL;
	gchar *path = NULL;
	gchar *sub_folder = NULL;
	const gchar *base_uri;
	ExchangeAccount *account = NULL;
	ESourceGroup *group;

	account = exchange_operations_get_exchange_account ();

	if (!account)
		return;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (target->selector));
	group = e_source_peek_group (source);
	base_uri = e_source_group_peek_base_uri (group);
	if (!base_uri || strcmp (base_uri, "exchange://"))
		return;

	uri = e_source_get_uri (source);
	path = g_strdup (uri + strlen ("exchange://") + strlen (account->account_filename) + strlen ("/;"));
	g_free (uri);
	sub_folder = strchr (path, '@');

	if (!sub_folder) {
		g_free (path);
		return;
	}

	for (i = 0; i < sizeof (popup_ab_items) / sizeof (popup_ab_items[0]); i++)
		menus = g_slist_prepend (menus, &popup_ab_items[i]);

	e_popup_add_items (target->target.popup, menus, NULL, popup_ab_free, target);
	g_free (path);

}

void
org_gnome_exchange_check_subscribed (EPlugin *ep, ECalPopupTargetSource *target)
{
	GSList *menus = NULL;
	gint i = 0;
	ESource *source = NULL;
	gchar *ruri = NULL;
	gchar *path = NULL;
	gchar *sub_folder = NULL;
	const gchar *base_uri;
	ExchangeAccount *account = NULL;
	ESourceGroup *group;

	account = exchange_operations_get_exchange_account ();

	if (!account)
		return;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (target->selector));
	group = e_source_peek_group (source);
	base_uri = e_source_group_peek_base_uri (group);
	if (!base_uri || strcmp (base_uri, "exchange://"))
		return;

	ruri = (gchar *) e_source_peek_relative_uri (source);
	path = g_strdup (ruri + strlen (account->account_filename) + strlen ("/;"));
	sub_folder = strchr (path, '@');

	if (!sub_folder) {
		g_free (path);
		return;
	}

	for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
		menus = g_slist_prepend (menus, &popup_items[i]);

	e_popup_add_items (target->target.popup, menus, NULL, popup_free, target);
	g_free (path);
}

static void
unsubscribe_dialog_ab_response (GtkDialog *dialog, gint response, gpointer data)
{

	if (response == GTK_RESPONSE_OK) {
		ExchangeAccount *account = NULL;
		gchar *path = NULL;
		gchar *uri = NULL;
		const gchar *source_uid = NULL;
		ESourceGroup *source_group = NULL;
		ESource *source = NULL;
		EABPopupTargetSource *target = data;

		account = exchange_operations_get_exchange_account ();

		if (!account)
			return;

		source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (target->selector));
		uri = e_source_get_uri (source);
		path = g_strdup (uri + strlen ("exchange://") + strlen (account->account_filename));
		source_uid = e_source_peek_uid (source);

		exchange_account_remove_shared_folder (account, path);

		source_group = e_source_peek_group (source);
		e_source_group_remove_source_by_uid (source_group, source_uid);
		g_free (path);
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
	if (response == GTK_RESPONSE_CANCEL)
		gtk_widget_destroy (GTK_WIDGET (dialog));
	if (response == GTK_RESPONSE_DELETE_EVENT)
		gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
unsubscribe_dialog_response (GtkDialog *dialog, gint response, gpointer data)
{

	if (response == GTK_RESPONSE_OK) {
		GSList *ids, *node_to_be_deleted;
		ExchangeAccount *account = NULL;
		gchar *path = NULL;
		gchar *ruri = NULL;
		const gchar *source_uid = NULL;
		GConfClient *client;
		ESourceGroup *source_group = NULL;
		ESource *source = NULL;
		ECalPopupTargetSource *target = data;

		client = gconf_client_get_default ();

		account = exchange_operations_get_exchange_account ();

		if (!account)
			return;

		source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (target->selector));
		ruri = (gchar *) e_source_peek_relative_uri (source);
		source_uid = e_source_peek_uid (source);

		path = g_strdup (ruri + strlen (account->account_filename));
		exchange_account_remove_shared_folder (account, path);
		ids = gconf_client_get_list (client,
					     CONF_KEY_SELECTED_CAL_SOURCES,
					     GCONF_VALUE_STRING, NULL);
		if (ids) {
			node_to_be_deleted = g_slist_find_custom (
						ids,
						source_uid,
						(GCompareFunc) strcmp);
			if (node_to_be_deleted) {
				g_free (node_to_be_deleted->data);
				ids = g_slist_delete_link (ids,
						node_to_be_deleted);
				gconf_client_set_list (client,
					CONF_KEY_SELECTED_CAL_SOURCES,
					GCONF_VALUE_STRING, ids, NULL);
			}
			g_slist_foreach (ids, (GFunc) g_free, NULL);
			g_slist_free (ids);
		}

		source_group = e_source_peek_group (source);
		e_source_group_remove_source_by_uid (source_group, source_uid);
		g_free (path);
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
	if (response == GTK_RESPONSE_CANCEL)
		gtk_widget_destroy (GTK_WIDGET (dialog));
	if (response == GTK_RESPONSE_DELETE_EVENT)
		gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
org_gnome_exchange_folder_ab_unsubscribe (EPopup *ep, EPopupItem *p, gpointer data)
{
	GtkWidget *dialog = NULL;
	EABPopupTargetSource *target = data;
	ESource *source = NULL;
	ExchangeAccount *account = NULL;
	gchar *title = NULL;
	gchar *displayed_folder_name = NULL;
	gint response;
	gint mode;
	ExchangeConfigListenerStatus status;

	account = exchange_operations_get_exchange_account ();

	if (!account)
		return;

	status = exchange_is_offline (&mode);

	if (status != CONFIG_LISTENER_STATUS_OK) {
		g_warning ("Config listener not found");
		return;
	} else if (mode == OFFLINE_MODE) {
		e_error_run (NULL, ERROR_DOMAIN ":account-offline-generic", NULL);
		return;
	}

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (target->selector));
	displayed_folder_name = (gchar *) e_source_peek_name (source);
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 _("Really unsubscribe from folder \"%s\"?"),
					 displayed_folder_name);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_REMOVE, GTK_RESPONSE_OK);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 6);

	title = g_strdup_printf (_("Unsubscribe from \"%s\""), displayed_folder_name);
	gtk_window_set_title (GTK_WINDOW (dialog), title);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	g_free (title);
	g_free (displayed_folder_name);

	gtk_widget_show (dialog);
	unsubscribe_dialog_ab_response (GTK_DIALOG (dialog), response, data);
}
void
org_gnome_exchange_folder_unsubscribe (EPopup *ep, EPopupItem *p, gpointer data)
{
	GtkWidget *dialog = NULL;
	ECalPopupTargetSource *target = data;
	ESource *source = NULL;
	ExchangeAccount *account = NULL;
	gchar *title = NULL;
	const gchar *displayed_folder_name;
	gint response;
	gint mode;
	ExchangeConfigListenerStatus status;

	account = exchange_operations_get_exchange_account ();

	if (!account)
		return;

	status = exchange_is_offline (&mode);

	if (status != CONFIG_LISTENER_STATUS_OK) {
		g_warning ("Config listener not found");
		return;
	} else if (mode == OFFLINE_MODE) {
		e_error_run (NULL, ERROR_DOMAIN ":account-offline-generic", NULL);
		return;
	}

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (target->selector));
	displayed_folder_name =  e_source_peek_name (source);
	dialog = gtk_message_dialog_new (NULL,
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 _("Really unsubscribe from folder \"%s\"?"),
					 displayed_folder_name);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_REMOVE, GTK_RESPONSE_OK);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 6);

	title = g_strdup_printf (_("Unsubscribe from \"%s\""), displayed_folder_name);
	gtk_window_set_title (GTK_WINDOW (dialog), title);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	g_free (title);

	gtk_widget_show (dialog);
	unsubscribe_dialog_response (GTK_DIALOG (dialog), response, data);
}

void
org_gnome_exchange_folder_subscription (EPlugin *ep, EMMenuTargetSelect *target, const gchar *fname)
{
	ExchangeAccount *account = NULL;
	gint mode;
	ExchangeConfigListenerStatus status;

	account = exchange_operations_get_exchange_account ();

	if (!account)
		return;

	status = exchange_is_offline (&mode);

	if (status != CONFIG_LISTENER_STATUS_OK) {
		g_warning ("Config listener not found");
		return;
	}
	else if (mode == OFFLINE_MODE) {
		/* Translators: this error code can be used for any operation
		 * (like subscribing to other user's folders, unsubscribing
		 * etc,) which can not be performed in offline mode
		 */
		e_error_run (NULL, ERROR_DOMAIN ":account-offline-generic", NULL);
		return;
	}

	create_folder_subscription_dialog (account, fname);
}

void
org_gnome_exchange_calendar_subscription (EPlugin *ep, EMMenuTargetSelect *target)
{
	const gchar *folder_name = N_("Calendar");
	org_gnome_exchange_folder_subscription (ep, target, folder_name);
}

void
org_gnome_exchange_addressbook_subscription (EPlugin *ep, EMMenuTargetSelect *target)
{
	const gchar *folder_name = N_("Contacts");
	org_gnome_exchange_folder_subscription (ep, target, folder_name);
}

void
org_gnome_exchange_tasks_subscription (EPlugin *ep, EMMenuTargetSelect *target)
{
	const gchar *folder_name = N_("Tasks");
	org_gnome_exchange_folder_subscription (ep, target, folder_name);
}

void
org_gnome_exchange_inbox_subscription (EPlugin *ep, EMMenuTargetSelect *target)
{
	const gchar *folder_name = N_("Inbox");
	org_gnome_exchange_folder_subscription (ep, target, folder_name);
}
