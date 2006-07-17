/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Author: 
 *  Sankar P ( psankar@novell.com )
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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

#include <mail/em-popup.h>
#include <mail/em-folder-view.h>
#include <glib/gi18n-lib.h>
#include <share-folder.h>
#include <e-gw-connection.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <e-util/e-error.h>

void org_gnome_retract_message (EPlugin *ep, EMPopupTargetSelect *t);


static void retract_mail_settings (EPopup *ep, EPopupItem *item, void *data)
{
	EGwConnection *cnc;
	CamelFolder *folder = (CamelFolder *)data;
	CamelStore *store = folder->parent_store;	
	char *id;
	
	cnc = get_cnc (store);	
	id = (char *)item->user_data;

	if (e_gw_connection_retract_request (cnc, id, NULL, FALSE, FALSE) != E_GW_CONNECTION_STATUS_OK )
		e_error_run (NULL, "org.gnome.evolution.message.retract:retract-failure", NULL);
	else {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, _("Message retracted successfully"));	
		gtk_dialog_run (GTK_DIALOG(dialog));
		gtk_widget_destroy (dialog);
	}
}

static EPopupItem popup_items[] = {
{ E_POPUP_ITEM, "50.emfv.06", N_("Retract Mail"), retract_mail_settings, NULL, NULL, 0, EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_LISTONLY}
};

static void popup_free (EPopup *ep, GSList *items, void *data)
{
	g_slist_free (items);
}

void org_gnome_retract_message (EPlugin *ep, EMPopupTargetSelect *t)
{
	GSList *menus = NULL;
	GPtrArray *uids;
	int i = 0;
	static int first = 0;

	uids = t->uids;
	if (g_strrstr (t->uri, "groupwise://") && !g_ascii_strcasecmp((t->folder)->full_name, "Sent Items")) {

		/* for translation*/
		if (!first) {
			popup_items[0].label =  _(popup_items[0].label);
			popup_items[0].user_data = g_strdup((char *) g_ptr_array_index(uids, 0));
		}

		first++;

		for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
			menus = g_slist_prepend (menus, &popup_items[i]);

		e_popup_add_items (t->target.popup, menus, NULL, popup_free, t->folder);
	} 
	return ;
}
