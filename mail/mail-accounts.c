/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include "mail-accounts.h"

#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-messagebox.h>
#include <gal/e-table/e-table-memory-store.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/util/e-unicode-i18n.h>
#include <camel/camel-url.h>

#include <bonobo/bonobo-generic-factory.h>

#include "mail.h"
#include "mail-config.h"
#include "mail-config-druid.h"
#include "mail-account-editor.h"
#include "mail-send-recv.h"

#include "art/mark.xpm"

static void mail_accounts_tab_class_init (MailAccountsTabClass *class);
static void mail_accounts_tab_init       (MailAccountsTab *prefs);
static void mail_accounts_tab_finalise   (GtkObject *obj);

static void mail_accounts_load (MailAccountsTab *tab);

static GdkPixbuf *pixbuf = NULL;

static GtkVBoxClass *parent_class = NULL;


GtkType
mail_accounts_tab_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MailAccountsTab",
			sizeof (MailAccountsTab),
			sizeof (MailAccountsTabClass),
			(GtkClassInitFunc) mail_accounts_tab_class_init,
			(GtkObjectInitFunc) mail_accounts_tab_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_vbox_get_type (), &type_info);
	}
	
	return type;
}

static void
mail_accounts_tab_class_init (MailAccountsTabClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) klass;
	parent_class = gtk_type_class (gtk_vbox_get_type ());
	
	object_class->finalize = mail_accounts_tab_finalise;
	/* override methods */
	
	
	/* setup static data */
	pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) mark_xpm);
}

static void
mail_accounts_tab_init (MailAccountsTab *prefs)
{
	prefs->druid = NULL;
	prefs->editor = NULL;
}

static void
mail_accounts_tab_finalise (GtkObject *obj)
{
	MailAccountsTab *prefs = (MailAccountsTab *) obj;
	
	gtk_object_unref (GTK_OBJECT (prefs->gui));
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

static void
account_add_finished (GtkWidget *widget, gpointer user_data)
{
	/* Either Cancel or Finished was clicked in the druid so reload the accounts */
	MailAccountsTab *prefs = user_data;
	
	prefs->druid = NULL;
	mail_accounts_load (prefs);
}

static void
account_add_clicked (GtkButton *button, gpointer user_data)
{
	MailAccountsTab *prefs = (MailAccountsTab *) user_data;
	
	if (prefs->druid == NULL) {
		prefs->druid = (GtkWidget *) mail_config_druid_new (prefs->shell);
		gtk_signal_connect (GTK_OBJECT (prefs->druid), "destroy",
				    GTK_SIGNAL_FUNC (account_add_finished), prefs);
		
		gtk_widget_show (prefs->druid);
	} else {
		gdk_window_raise (prefs->druid->window);
	}
}

static void
account_edit_finished (GtkWidget *widget, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	
	prefs->editor = NULL;
	mail_accounts_load (prefs);
}

static void
account_edit_clicked (GtkButton *button, gpointer user_data)
{
	MailAccountsTab *prefs = (MailAccountsTab *) user_data;
	
	if (prefs->editor == NULL) {
		int row;
		
		row = e_table_get_cursor_row (prefs->table);
		if (row >= 0) {
			MailConfigAccount *account;
			GtkWidget *window;
			
			window = gtk_widget_get_ancestor (GTK_WIDGET (prefs), GTK_TYPE_WINDOW);
			
			account = e_table_memory_get_data (E_TABLE_MEMORY (prefs->model), row);
			prefs->editor = (GtkWidget *) mail_account_editor_new (account, GTK_WINDOW (window), prefs);
			gtk_signal_connect (GTK_OBJECT (prefs->editor), "destroy",
					    GTK_SIGNAL_FUNC (account_edit_finished),
					    prefs);
			gtk_widget_show (prefs->editor);
		}
	} else {
		gdk_window_raise (prefs->editor->window);
	}
}

static void
account_delete_clicked (GtkButton *button, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	const MailConfigAccount *account;
	GnomeDialog *confirm;
	const GSList *list;
	int row, ans;
	
	row = e_table_get_cursor_row (prefs->table);
	
	/* make sure we have a valid account selected and that we aren't editing anything... */
	if (row < 0 || prefs->editor != NULL)
		return;
	
	confirm = GNOME_DIALOG (gnome_message_box_new (_("Are you sure you want to delete this account?"),
						       GNOME_MESSAGE_BOX_QUESTION,
						       NULL));
	gnome_dialog_append_button_with_pixmap (confirm, _("Delete"), GNOME_STOCK_BUTTON_YES);
	gnome_dialog_append_button_with_pixmap (confirm, _("Don't delete"), GNOME_STOCK_BUTTON_NO);
	gtk_window_set_policy (GTK_WINDOW (confirm), TRUE, TRUE, TRUE);
	gtk_window_set_modal (GTK_WINDOW (confirm), TRUE);
	gtk_window_set_title (GTK_WINDOW (confirm), _("Really delete account?"));
	gnome_dialog_set_parent (confirm, GTK_WINDOW (prefs));
	ans = gnome_dialog_run_and_close (confirm);
	
	if (ans == 0) {
		int select, len;
		
		account = e_table_memory_get_data (E_TABLE_MEMORY (prefs->model), row);
		
		/* remove it from the folder-tree in the shell */
		if (account->source && account->source->url && account->source->enabled)
			mail_remove_storage_by_uri (account->source->url);
		
		/* remove it from the config file */
		list = mail_config_remove_account ((MailConfigAccount *) account);
		
		mail_config_write ();
		
		mail_autoreceive_setup ();
		
		e_table_memory_store_remove (E_TABLE_MEMORY_STORE (prefs->model), row);
		
		len = list ? g_slist_length ((GSList *) list) : 0;
		if (len > 0) {
			select = row >= len ? len - 1 : row;
			e_table_set_cursor_row (prefs->table, select);
		} else {
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_edit), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_delete), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_default), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_able), FALSE);
		}
	}
}

static void
account_default_clicked (GtkButton *button, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	const MailConfigAccount *account;
	int row;
	
	row = e_table_get_cursor_row (prefs->table);
	
	if (row >= 0) {
		account = e_table_memory_get_data (E_TABLE_MEMORY (prefs->model), row);
		
		mail_config_set_default_account (account);
		
		mail_config_write ();
		
		mail_accounts_load (prefs);
	}
}

static void
account_able_clicked (GtkButton *button, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	const MailConfigAccount *account;
	int row;
	
	row = e_table_get_cursor_row (prefs->table);
	
	if (row >= 0) {
		account = e_table_memory_get_data (E_TABLE_MEMORY (prefs->model), row);
		account->source->enabled = !account->source->enabled;
		
		if (account->source && account->source->url) {
			if (account->source->enabled)
				mail_load_storage_by_uri (prefs->shell, account->source->url, account->name);
			else
				mail_remove_storage_by_uri (account->source->url);
		}
		
		mail_autoreceive_setup ();
		
		mail_config_write ();
		
		mail_accounts_load (prefs);
	}
}

static void
account_cursor_change (ETable *table, int row, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	
	if (row >= 0) {
		const MailConfigAccount *account;
		
		account = e_table_memory_get_data (E_TABLE_MEMORY (prefs->model), row);
		if (account->source && account->source->enabled)
			gtk_label_set_text (GTK_LABEL (GTK_BIN (prefs->mail_able)->child), _("Disable"));
		else
			gtk_label_set_text (GTK_LABEL (GTK_BIN (prefs->mail_able)->child), _("Enable"));
		
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_edit), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_delete), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_default), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_able), TRUE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_edit), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_delete), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_default), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_able), FALSE);
		
		gtk_widget_grab_focus (GTK_WIDGET (prefs->mail_add));
	}
}

static void
account_double_click (ETable *table, int row, int col, GdkEvent *event, gpointer user_data)
{
	account_edit_clicked (NULL, user_data);
}

static void
mail_accounts_load (MailAccountsTab *prefs)
{
	const GSList *node;
	int row = 0;
	
	e_table_memory_freeze (E_TABLE_MEMORY (prefs->model));
	
	e_table_memory_store_clear (E_TABLE_MEMORY_STORE (prefs->model));
	
	node = mail_config_get_accounts ();
	while (node) {
		const MailConfigAccount *account;
		CamelURL *url;
		
		account = node->data;
		
		url = account->source && account->source->url ? camel_url_new (account->source->url, NULL) : NULL;
		
		e_table_memory_store_insert_list (E_TABLE_MEMORY_STORE (prefs->model),
						  row, GINT_TO_POINTER (account->source->enabled),
						  account->name,
						  url && url->protocol ? url->protocol : U_("None"));
		
		if (url)
			camel_url_free (url);
		
		e_table_memory_set_data (E_TABLE_MEMORY (prefs->model), row, (gpointer) account);
		
		node = node->next;
		row++;
	}
	
	e_table_memory_thaw (E_TABLE_MEMORY (prefs->model));
}


GtkWidget *mail_accounts_etable_new (char *widget_name, char *string1, char *string2,
				     int int1, int int2);

GtkWidget *
mail_accounts_etable_new (char *widget_name, char *string1, char *string2, int int1, int int2)
{
	ETable *etable;
	ETableModel *model;
	ETableExtras *extras;
	GdkPixbuf *images[2];
	ETableMemoryStoreColumnInfo columns[] = {
		E_TABLE_MEMORY_STORE_INTEGER,
		E_TABLE_MEMORY_STORE_STRING,
		E_TABLE_MEMORY_STORE_STRING,
		E_TABLE_MEMORY_STORE_TERMINATOR,
	};
	
	extras = e_table_extras_new ();
	
	images[0] = NULL;   /* disabled */
	images[1] = pixbuf; /* enabled */
	e_table_extras_add_cell (extras, "render_able", e_cell_toggle_new (0, 2, images));
	
	model = e_table_memory_store_new (columns);
	
	etable = (ETable *) e_table_new_from_spec_file (model, extras, EVOLUTION_ETSPECDIR "/mail-accounts.etspec", NULL);
	
	return (GtkWidget *) etable;
}

static void
mail_accounts_tab_construct (MailAccountsTab *prefs)
{
	GtkWidget *toplevel;
	GladeXML *gui;
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "accounts_tab");
	prefs->gui = gui;
	
	/* get our toplevel widget */
	toplevel = glade_xml_get_widget (gui, "toplevel");
	
	/* reparent */
	gtk_widget_ref (toplevel);
	gtk_widget_unparent (toplevel);
	gtk_widget_set_parent (toplevel, GTK_WIDGET (prefs));
	gtk_widget_unref (toplevel);
	
	prefs->table = E_TABLE (glade_xml_get_widget (gui, "etableMailAccounts"));
	prefs->model = prefs->table->model;
	
	gtk_signal_connect (GTK_OBJECT (prefs->table), "cursor_change",
			    account_cursor_change, prefs);
	
	gtk_signal_connect (GTK_OBJECT (prefs->table), "double_click",
			    account_double_click, prefs);
	
	mail_accounts_load (prefs);
	
	prefs->mail_add = GTK_BUTTON (glade_xml_get_widget (gui, "cmdAccountAdd"));
	gtk_signal_connect (GTK_OBJECT (prefs->mail_add), "clicked",
			    account_add_clicked, prefs);
	
	prefs->mail_edit = GTK_BUTTON (glade_xml_get_widget (gui, "cmdAccountEdit"));
	gtk_signal_connect (GTK_OBJECT (prefs->mail_edit), "clicked",
			    account_edit_clicked, prefs);
	
	prefs->mail_delete = GTK_BUTTON (glade_xml_get_widget (gui, "cmdAccountDelete"));
	gtk_signal_connect (GTK_OBJECT (prefs->mail_delete), "clicked",
			    account_delete_clicked, prefs);
	
	prefs->mail_default = GTK_BUTTON (glade_xml_get_widget (gui, "cmdAccountDefault"));
	gtk_signal_connect (GTK_OBJECT (prefs->mail_default), "clicked",
			    account_default_clicked, prefs);
	
	prefs->mail_able = GTK_BUTTON (glade_xml_get_widget (gui, "cmdAccountAble"));
	gtk_signal_connect (GTK_OBJECT (prefs->mail_able), "clicked",
			    account_able_clicked, prefs);
}


GtkWidget *
mail_accounts_tab_new (GNOME_Evolution_Shell shell)
{
	MailAccountsTab *new;
	
	new = (MailAccountsTab *) gtk_type_new (mail_accounts_tab_get_type ());
	mail_accounts_tab_construct (new);
	new->shell = shell;
	
	return (GtkWidget *) new;
}


void
mail_accounts_tab_apply (MailAccountsTab *prefs)
{
	/* nothing to do here... */
}
