/*
 *
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
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <exchange-account.h>
#include <e-util/e-dialog-utils.h>
#include <calendar/gui/e-cal-popup.h>
#include <libedataserverui/e-source-selector.h>
#include <camel/camel-url.h>
#include <mail/em-popup.h>
#include <mail/em-menu.h>
#include <libebook/e-book.h>
#include "exchange-config-listener.h"
#include "exchange-operations.h"
#include "exchange-permissions-dialog.h"
#include "addressbook/gui/widgets/eab-popup.h"
#include "calendar/gui/e-cal-menu.h"
#include "calendar/gui/e-cal-model.h"
#include "addressbook/gui/widgets/eab-menu.h"

#define d(x)

static void org_folder_permissions_cb (EPopup *ep, EPopupItem *p, gpointer data);
void org_gnome_exchange_folder_permissions (EPlugin *ep, EMPopupTargetFolder *t);
void org_gnome_exchange_menu_folder_permissions (EPlugin *ep, EMMenuTargetSelect *target);
void org_gnome_exchange_calendar_permissions (EPlugin *ep, ECalPopupTargetSource *target);
void org_gnome_exchange_addressbook_permissions (EPlugin *ep, EABPopupTargetSource *target);
void org_gnome_exchange_menu_ab_permissions (EPlugin *ep, EABMenuTargetSelect *target);
void org_gnome_exchange_menu_tasks_permissions (EPlugin *ep, ECalMenuTargetSelect *target);
void org_gnome_exchange_menu_cal_permissions (EPlugin *ep, ECalMenuTargetSelect *target);

gchar *selected_exchange_folder_uri = NULL;

static EPopupItem popup_items[] = {
	{ E_POPUP_ITEM, (gchar *) "30.emc.10", (gchar *) N_("Permissions..."), org_folder_permissions_cb, NULL, (gchar *) "folder-new", 0, EM_POPUP_FOLDER_INFERIORS }
};

static void
popup_free (EPopup *ep, GSList *items, gpointer data)
{
       g_slist_free (items);
}

void
org_gnome_exchange_calendar_permissions (EPlugin *ep, ECalPopupTargetSource *target)
{
	GSList *menus = NULL;
	gint i = 0, mode;
	static gint first =0;
	ExchangeAccount *account = NULL;
	ESource *source = NULL;
	gchar *uri = NULL;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (target->selector));
	uri = (gchar *) e_source_get_uri (source);
	if (uri && ! g_strrstr (uri, "exchange://"))	{
		return;
	}

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;
	exchange_account_is_offline (account, &mode);
	if (mode == OFFLINE_MODE)
		return;
	if (!exchange_account_get_folder (account, uri))
		return;

	selected_exchange_folder_uri = uri;

	/* for translation*/
	if (!first) {
		popup_items[0].label =  _(popup_items[0].label);
		first++;

	}

	for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
		menus = g_slist_prepend (menus, &popup_items[i]);

	e_popup_add_items (target->target.popup, menus, NULL, popup_free, NULL);

}

void
org_gnome_exchange_addressbook_permissions (EPlugin *ep, EABPopupTargetSource *target)
{
	GSList *menus = NULL;
	gint i = 0, mode;
	static gint first =0;
	ExchangeAccount *account = NULL;
	ESource *source = NULL;
	gchar *uri = NULL;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (target->selector));
	uri = (gchar *) e_source_get_uri (source);
	if (!g_strrstr (uri, "exchange://"))
		return;

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;
	exchange_account_is_offline (account, &mode);
	if (mode == OFFLINE_MODE)
		return;

	if (!exchange_account_get_folder (account, uri))
		return;

	selected_exchange_folder_uri = uri;

	/* for translation*/
	if (!first) {
		popup_items[0].label =  _(popup_items[0].label);
		first++;
	}

	for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
		menus = g_slist_prepend (menus, &popup_items[i]);

	e_popup_add_items (target->target.popup, menus, NULL, popup_free, NULL);
}

void
org_gnome_exchange_folder_permissions (EPlugin *ep, EMPopupTargetFolder *target)
{
	GSList *menus = NULL;
	gint i = 0, mode;
	static gint first =0;
	gchar *path = NULL;
	gchar *fixed_path = NULL;
	ExchangeAccount *account = NULL;

	d(g_print ("exchange-folder-permission.c: entry\n"));

	if (!g_strrstr (target->uri, "exchange://"))
		return;

	account = exchange_operations_get_exchange_account ();
	if (!account )
		return;
	exchange_account_is_offline (account, &mode);
	if (mode == OFFLINE_MODE)
		return;

	if (strlen (target->uri) <= strlen ("exchange://") + strlen (account->account_filename))
		return;

	path = target->uri + strlen ("exchange://") + strlen (account->account_filename);

	if (!path || !*path)
		return;

	fixed_path = camel_url_decode_path (path);
	d(g_print ("exchange-folder-permission.c: path=[%s], fixed_path=[%s]\n", path, fixed_path));

	if (! g_strrstr (target->uri, "exchange://") ||
	    !exchange_account_get_folder (account, fixed_path)) {
		g_free (fixed_path);
		return;
	}

	g_free (fixed_path);

	selected_exchange_folder_uri = path;
	/* for translation*/
	if (!first) {
		popup_items[0].label =  _(popup_items[0].label);
		first++;
	}

	for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
		menus = g_slist_prepend (menus, &popup_items[i]);

	e_popup_add_items (target->target.popup, menus, NULL, popup_free, NULL);

}

static void
org_folder_permissions_cb (EPopup *ep, EPopupItem *p, gpointer data)
{
	ExchangeAccount *account = NULL;
	EFolder *folder = NULL;

	account = exchange_operations_get_exchange_account ();

	if (!account)
		return;

	folder = exchange_account_get_folder (account, selected_exchange_folder_uri);
	if (folder)
		exchange_permissions_dialog_new (account, folder, NULL);

}

void
org_gnome_exchange_menu_folder_permissions (EPlugin *ep, EMMenuTargetSelect *target)
{
	ExchangeAccount *account = NULL;
	EFolder *folder = NULL;
	gchar *path = NULL;
	gint mode;

	if (!g_str_has_prefix (target->uri, "exchange://"))
		return;

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;
	exchange_account_is_offline (account, &mode);
	if (mode == OFFLINE_MODE)
		return;

	path = target->uri + strlen ("exchange://") + strlen (account->account_filename);
	folder = exchange_account_get_folder (account, path);
	if (folder)
		exchange_permissions_dialog_new (account, folder, NULL);
}

void
org_gnome_exchange_menu_cal_permissions (EPlugin *ep, ECalMenuTargetSelect *target)
{
	ExchangeAccount *account = NULL;
	EFolder *folder = NULL;
	ECalModel *model = NULL;
	ECal *ecal = NULL;
	gchar *uri = NULL;
	gint mode;

	if (!target)
		return;
	if (target->model)
		model = E_CAL_MODEL (target->model);

	ecal = e_cal_model_get_default_client (model);
	uri = (gchar *) e_cal_get_uri (ecal);
	if (!uri)
		return;
	else
		if (!g_str_has_prefix (uri, "exchange://"))
			return;

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;
	exchange_account_is_offline (account, &mode);
	if (mode == OFFLINE_MODE)
		return;

	folder = exchange_account_get_folder (account, uri);
	exchange_permissions_dialog_new (account, folder, NULL);
}

void
org_gnome_exchange_menu_tasks_permissions (EPlugin *ep, ECalMenuTargetSelect *target)
{
	ExchangeAccount *account = NULL;
	EFolder *folder = NULL;
	ECalModel *model = NULL;
	ECal *ecal = NULL;
	gchar *uri = NULL;
	gint mode;

	if (!target)
		return;
	if (target->model)
		model = E_CAL_MODEL (target->model);

	ecal = e_cal_model_get_default_client (model);
	uri = (gchar *) e_cal_get_uri (ecal);
	if (!uri)
		return;
	else
		if (!g_str_has_prefix (uri, "exchange://"))
			return;
	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;
	exchange_account_is_offline (account, &mode);
	if (mode == OFFLINE_MODE)
		return;

	folder = exchange_account_get_folder (account, uri);
	exchange_permissions_dialog_new (account, folder, NULL);
}

void
org_gnome_exchange_menu_ab_permissions (EPlugin *ep, EABMenuTargetSelect *target)
{
	ExchangeAccount *account = NULL;
	EFolder *folder = NULL;
	EBook *ebook = NULL;
	gchar *uri = NULL;
	gint mode;

	if (!target)
		return;
	if (target->book)
		ebook = E_BOOK (target->book);

	uri = (gchar *) e_book_get_uri (ebook);
	if (!uri)
		return;
	else
		if (!g_str_has_prefix (uri, "exchange://"))
			return;

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;
	exchange_account_is_offline (account, &mode);
	if (mode == OFFLINE_MODE)
		return;

	folder = exchange_account_get_folder (account, uri);
	exchange_permissions_dialog_new (account, folder, NULL);
}
