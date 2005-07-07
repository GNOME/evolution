/*
 * Copyright (C) 2005 Novell, Inc.
 *
 * Author(s): Vivek Jain <jvivek@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdio.h>
#include <gtk/gtk.h>

#include "camel/camel-store.h"
#include "camel/camel-folder.h"
#include "camel/camel-medium.h"
#include "camel/camel-mime-message.h"
#include "mail/em-popup.h"
#include <mail/em-folder-view.h>
#include <e-gw-connection.h>
#include "mail/em-account-editor.h"
#include "e-util/e-account.h"
#include "mail/em-config.h"
#include "share-folder.h"
#include "junk-settings.h"

void 
org_gnome_junk_settings(EPlugin *ep, EMPopupTargetSelect *t);

static void
abort_changes (JunkSettings *js)
{
       	g_object_run_dispose ((GObject *)js);
}

static void
junk_dialog_response (GtkWidget *dialog, int response, JunkSettings *js)
{
	if (response == GTK_RESPONSE_ACCEPT) {
		commit_changes(js);
		abort_changes (js);
	}
	else
		abort_changes (js);

	gtk_widget_destroy (dialog);

}

static void 
junk_mail_settings (EPopup *ep, EPopupItem *item, void *data)
{
	GtkWidget *dialog ,*w, *notebook, *box;
	JunkSettings *junk_tab;
	int page_count =0;
	EGwConnection *cnc;
	CamelFolder *folder = (CamelFolder *)data;
	CamelStore *store = folder->parent_store;	
	cnc = get_cnc (store);	

	dialog =  gtk_dialog_new_with_buttons ("Junk Settings",
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL,
			GTK_RESPONSE_REJECT,
			GTK_STOCK_OK,
			GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_window_set_default_size ((GtkWindow *) dialog, 292, 260);
	gtk_widget_ensure_style (dialog);
	gtk_container_set_border_width ((GtkContainer *) ((GtkDialog *) dialog)->vbox, 12);
	box = gtk_vbox_new (FALSE, 6);
	w = gtk_label_new ();		
	gtk_label_set_markup (GTK_LABEL (w), "<b>Junk Mail Settings</b>");
	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 6);

	junk_tab = junk_settings_new (cnc);
	w = junk_tab->vbox;
	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 6);
	
	/*We might have to add more options for settings i.e. more pages*/
	while (page_count > 0 ){
		notebook = gtk_notebook_new ();
		gtk_notebook_append_page (notebook, box, NULL);
		gtk_box_pack_start ((GtkBox *) ((GtkDialog *) dialog)->vbox, notebook, TRUE, TRUE, 0);
	}
	
	if (page_count == 0)
		gtk_box_pack_start ((GtkBox *) ((GtkDialog *) dialog)->vbox, box, TRUE, TRUE, 0);

	g_signal_connect (dialog, "response", G_CALLBACK (junk_dialog_response), junk_tab);
	gtk_widget_show_all (dialog);
}

static EPopupItem popup_items[] = {
{ E_POPUP_ITEM, "50.emfv.05", N_("Junk Mail Settings..."), junk_mail_settings, NULL, NULL, 0, EM_POPUP_SELECT_MANY|EM_FOLDER_VIEW_SELECT_LISTONLY}
};

static void 
popup_free (EPopup *ep, GSList *items, void *data)
{
g_slist_free (items);
}

void 
org_gnome_junk_settings(EPlugin *ep, EMPopupTargetSelect *t)
{
	GSList *menus = NULL;
	
	int i = 0;
	static int first =1;
	
	if (! g_strrstr (t->uri, "groupwise://"))
		return ;
	
	/* for translation*/
	if (first == 1) {
		popup_items[0].label =  _(popup_items[0].label);
	
	}
	
	first++;
	
	for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
		menus = g_slist_prepend (menus, &popup_items[i]);
	
	e_popup_add_items (t->target.popup, menus, NULL, popup_free, t->folder);
       	
}

