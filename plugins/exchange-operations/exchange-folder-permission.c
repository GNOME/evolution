/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Shakti Sen <shprasad@novell.com>
 *  Copyright (C) 2005 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <gtk/gtkdialog.h>
#include <gconf/gconf-client.h>
#include <libedataserver/e-xml-hash-utils.h>
#include <exchange/exchange-account.h>
#include <e-util/e-dialog-utils.h>
#include "exchange-config-listener.h"
#include "exchange-operations.h"
#include <mail/em-popup.h>
#include <mail/em-menu.h>

static void org_folder_permissions_cb (EPopup *ep, EPopupItem *p, void *data);
void org_gnome_exchange_folder_permissions (EPlugin *ep, EMPopupTargetFolder *t);
void org_gnome_exchange_menu_folder_permissions (EPlugin *ep, EMMenuTargetSelect *target);

gchar *selected_exchange_folder_uri = NULL;

static EPopupItem popup_items[] = {
	{ E_POPUP_ITEM, "40.emc.30", N_("Permissions..."), org_folder_permissions_cb, NULL, "stock_new-dir", 0, EM_POPUP_FOLDER_INFERIORS }
};

static void
popup_free (EPopup *ep, GSList *items, void *data)
{
       g_slist_free (items);
}

void
org_gnome_exchange_folder_permissions (EPlugin *ep, EMPopupTargetFolder *t)
{
	GSList *menus = NULL;
	int i = 0;
	static int first =1;
	GSList *accounts, *acc;
	ExchangeAccount *account = NULL;
	EFolder *folder = NULL;

	accounts = exchange_config_listener_get_accounts (exchange_global_config_listener);
	for (acc = accounts; acc;  acc = acc->next) {
		account = acc->data;
	}

	folder = exchange_account_get_folder (account, t->uri);


	if (!folder)
		return;

	if (! g_strrstr (t->uri, "exchange://") && !folder)
		return ;

	selected_exchange_folder_uri = t->uri;
	/* for translation*/
	if (first) {
		popup_items[0].label =  _(popup_items[0].label);

	}

	first++;

	for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
		menus = g_slist_prepend (menus, &popup_items[i]);

	e_popup_add_items (t->target.popup, menus, NULL, popup_free, NULL);

}

static void
org_folder_permissions_cb (EPopup *ep, EPopupItem *p, void *data)
{
	GSList *accounts, *acc;
	ExchangeAccount *account = NULL;
	EFolder *folder = NULL;

	accounts = exchange_config_listener_get_accounts (exchange_global_config_listener);
	for (acc = accounts; acc;  acc = acc->next) {
		account = acc->data;
	}

	folder = exchange_account_get_folder (account, selected_exchange_folder_uri);
	if (folder)
		exchange_permissions_dialog_new (account, folder, NULL);

}

void
org_gnome_exchange_menu_folder_permissions (EPlugin *ep, EMMenuTargetSelect *target)
{
	GSList *accounts, *acc;
	ExchangeAccount *account = NULL;
	EFolder *folder = NULL;

	if (target == NULL)
		return;

	accounts = exchange_config_listener_get_accounts (exchange_global_config_listener);
	for (acc = accounts; acc;  acc = acc->next) {
		account = acc->data;
	}

	folder = exchange_account_get_folder (account, target->uri);
	if (folder)
		exchange_permissions_dialog_new (account, folder, NULL);
}
