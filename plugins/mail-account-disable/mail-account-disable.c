/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Shreyas Srinivasan <sshreyas@novell.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
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
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <mail/mail-component.h>
#include <mail/em-folder-selector.h>
#include <mail/em-popup.h>
#include <mail/em-account-editor.h>
#include <mail/mail-config.h>
#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

#define ACCOUNT_DISABLE 0
#define PROXY_LOGOUT 1

void mail_account_disable (EPopup *ep, EPopupItem *p, void *data);
void org_gnome_create_mail_account_disable (EPlugin *ep, EMPopupTargetFolder *t);

static EPopupItem popup_items[] = {
	{ E_POPUP_ITEM, "20.emc.04", N_("_Disable"), mail_account_disable, NULL, NULL, 0, EM_POPUP_FOLDER_STORE },
	{ E_POPUP_ITEM, "20.emc.04", N_("Proxy _Logout"), mail_account_disable, NULL, NULL, 0, EM_POPUP_FOLDER_STORE }
};

static void 
popup_free (EPopup *ep, GSList *items, void *data)
{
	g_slist_free (items);
}

void
mail_account_disable (EPopup *ep, EPopupItem *p, void *data)
{
	char *uri = data;
	EAccount *account;

	account = mail_config_get_account_by_source_url (uri);

	if (account) {
		MailComponent *component = mail_component_peek ();
		
		if (mail_config_has_proxies(account))
			mail_config_remove_account_proxies (account);
		
		account->enabled = !account->enabled;
		e_account_list_change(mail_config_get_accounts(), account);
		mail_component_remove_store_by_uri (component, account->source->url);		

		if (account->parent_uid)
			mail_config_remove_account (account);

		mail_config_save_accounts();	
	}	

	return ;
}

void
org_gnome_create_mail_account_disable (EPlugin *ep, EMPopupTargetFolder *t)
{
	EAccount *account;
	GSList *menus = NULL;

	account = mail_config_get_account_by_source_url (t->uri);

	if (g_strrstr (t->uri,"groupwise://") && account->parent_uid) {
		popup_items[PROXY_LOGOUT].label =  _(popup_items [PROXY_LOGOUT].label);
		menus = g_slist_prepend (menus, &popup_items [PROXY_LOGOUT]);
	}
	else {
		popup_items[ACCOUNT_DISABLE].label =  _(popup_items [ACCOUNT_DISABLE].label);
		menus = g_slist_prepend (menus, &popup_items [ACCOUNT_DISABLE]);
	}

	e_popup_add_items (t->target.popup, menus, NULL, popup_free, t->uri);	

	return;
}

