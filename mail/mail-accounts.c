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
#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/util/e-unicode-i18n.h>
#include <gal/widgets/e-unicode.h>
#include <camel/camel-url.h>

#include <bonobo/bonobo-generic-factory.h>

#include "mail.h"
#include "mail-config.h"
#include "mail-config-druid.h"
#include "mail-account-editor.h"
#include "mail-account-editor-news.h"
#include "mail-send-recv.h"

#include "art/mark.xpm"

#define USE_ETABLE 0

static void mail_accounts_tab_class_init (MailAccountsTabClass *class);
static void mail_accounts_tab_init       (MailAccountsTab *prefs);
static void mail_accounts_tab_finalise   (GtkObject *obj);

static void mail_accounts_load (MailAccountsTab *tab);

static GdkPixbuf *disabled_pixbuf = NULL;
static GdkPixbuf *enabled_pixbuf = NULL;

static GtkVBoxClass *parent_class = NULL;


#define PREFS_WINDOW(prefs) GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (prefs), GTK_TYPE_WINDOW))


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
	disabled_pixbuf = NULL;
	enabled_pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) mark_xpm);
}

static void
mail_accounts_tab_init (MailAccountsTab *prefs)
{
	prefs->druid = NULL;
	prefs->editor = NULL;
	
	gdk_pixbuf_render_pixmap_and_mask (enabled_pixbuf, &prefs->mark_pixmap, &prefs->mark_bitmap, 128);
}

static void
mail_accounts_tab_finalise (GtkObject *obj)
{
	MailAccountsTab *prefs = (MailAccountsTab *) obj;
	
	gtk_object_unref (GTK_OBJECT (prefs->gui));
	gdk_pixmap_unref (prefs->mark_pixmap);
	gdk_bitmap_unref (prefs->mark_bitmap);
	
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
#if USE_ETABLE		
		row = e_table_get_cursor_row (prefs->table);
#else
		row = prefs->table->selection ? GPOINTER_TO_INT (prefs->table->selection->data) : -1;
#endif
		if (row >= 0) {
			MailConfigAccount *account;
			GtkWidget *window;
			
			window = gtk_widget_get_ancestor (GTK_WIDGET (prefs), GTK_TYPE_WINDOW);
			
#if USE_ETABLE
			account = e_table_memory_get_data (E_TABLE_MEMORY (prefs->model), row);
#else
			account = gtk_clist_get_row_data (prefs->table, row);
#endif
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
	
#if USE_ETABLE
	row = e_table_get_cursor_row (prefs->table);
#else
	row = prefs->table->selection ? GPOINTER_TO_INT (prefs->table->selection->data) : -1;
#endif
	
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
	gnome_dialog_set_parent (confirm, PREFS_WINDOW (prefs));
	ans = gnome_dialog_run_and_close (confirm);
	
	if (ans == 0) {
		int select, len;
		
#if USE_ETABLE
		account = e_table_memory_get_data (E_TABLE_MEMORY (prefs->model), row);
#else
		account = gtk_clist_get_row_data (prefs->table, row);
#endif
		
		/* remove it from the folder-tree in the shell */
		if (account->source && account->source->url && account->source->enabled)
			mail_remove_storage_by_uri (account->source->url);
		
		/* remove it from the config file */
		list = mail_config_remove_account ((MailConfigAccount *) account);
		
		mail_config_write ();
		
		mail_autoreceive_setup ();
		
#if USE_ETABLE
		e_table_memory_store_remove (E_TABLE_MEMORY_STORE (prefs->model), row);
#else
		gtk_clist_remove (prefs->table, row);
#endif		
		
		len = list ? g_slist_length ((GSList *) list) : 0;
		if (len > 0) {
			select = row >= len ? len - 1 : row;
#if USE_ETABLE
			e_table_set_cursor_row (prefs->table, select);
#else
			gtk_clist_select_row (prefs->table, select, 0);
#endif
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
	
#if USE_ETABLE
	row = e_table_get_cursor_row (prefs->table);
#else
	row = prefs->table->selection ? GPOINTER_TO_INT (prefs->table->selection->data) : -1;
#endif
	
	if (row >= 0) {
#if USE_ETABLE
		account = e_table_memory_get_data (E_TABLE_MEMORY (prefs->model), row);
#else
		account = gtk_clist_get_row_data (prefs->table, row);
#endif
		
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
	
#if USE_ETABLE
	row = e_table_get_cursor_row (prefs->table);
#else
	row = prefs->table->selection ? GPOINTER_TO_INT (prefs->table->selection->data) : -1;
#endif
	
	if (row >= 0) {
#if USE_ETABLE
		account = e_table_memory_get_data (E_TABLE_MEMORY (prefs->model), row);
#else
		account = gtk_clist_get_row_data (prefs->table, row);
#endif
		
		account->source->enabled = !account->source->enabled;
		
#if 0
		/* if the account got disabled, remove it from the folder-tree, otherwise add it to the folder-tree */
		if (account->source && account->source->url) {
			if (account->source->enabled)
				mail_load_storage_by_uri (prefs->shell, account->source->url, account->name);
			else
				mail_remove_storage_by_uri (account->source->url);
		}
#endif
		
#if USE_ETABLE
		
#else	
		if (account->source->enabled)
			gtk_clist_set_pixmap (prefs->table, row, 0, 
					      prefs->mark_pixmap, 
					      prefs->mark_bitmap);
		else
			gtk_clist_set_pixmap (prefs->table, row, 0, NULL, NULL);
		
		gtk_clist_select_row (prefs->table, row, 0);
#endif
		
		mail_autoreceive_setup ();
		
		mail_config_write ();
	}
}

#if USE_ETABLE
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
#else
static void
account_cursor_change (GtkCList *table, int row, int column, GdkEventButton *event, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	
	if (row >= 0) {
		const MailConfigAccount *account;
		
		account = gtk_clist_get_row_data (prefs->table, row);
		if (account->source && account->source->enabled)
			gtk_label_set_text (GTK_LABEL (GTK_BIN (prefs->mail_able)->child), _("Disable"));
		else
			gtk_label_set_text (GTK_LABEL (GTK_BIN (prefs->mail_able)->child), _("Enable"));
		
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_edit), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_delete), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_default), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_able), TRUE);
		
		if (event && event->type == GDK_2BUTTON_PRESS)
			account_edit_clicked (NULL, user_data);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_edit), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_delete), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_default), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (prefs->mail_able), FALSE);
		
		gtk_widget_grab_focus (GTK_WIDGET (prefs->mail_add));
	}
}
#endif


static void
mail_accounts_load (MailAccountsTab *prefs)
{
	const GSList *node;
	int row = 0;
	
#if USE_ETABLE
	e_table_memory_freeze (E_TABLE_MEMORY (prefs->model));
	
	e_table_memory_store_clear (E_TABLE_MEMORY_STORE (prefs->model));
#else
	gtk_clist_freeze (prefs->table);
	
	gtk_clist_clear (prefs->table);
#endif
	
	node = mail_config_get_accounts ();
	while (node) {
		const MailConfigAccount *account;
		CamelURL *url;
		
		account = node->data;
		
		url = account->source && account->source->url ? camel_url_new (account->source->url, NULL) : NULL;
		
#if USE_ETABLE
		e_table_memory_store_insert_list (E_TABLE_MEMORY_STORE (prefs->model),
						  row, GINT_TO_POINTER (account->source->enabled),
						  account->name,
						  url && url->protocol ? url->protocol : U_("None"));
		
		e_table_memory_set_data (E_TABLE_MEMORY (prefs->model), row, (gpointer) account);
#else
		{
			char *text[3];
			
			text[0] = NULL;
			text[1] = e_utf8_to_gtk_string (GTK_WIDGET (prefs->table), account->name);
			text[2] = url && url->protocol ? url->protocol : (char *) _("None");
			
			gtk_clist_insert (prefs->table, row, text);
			
			g_free (text[1]);
			
			if (account->source->enabled)
				gtk_clist_set_pixmap (prefs->table, row, 0, 
						      prefs->mark_pixmap, 
						      prefs->mark_bitmap);
			
			gtk_clist_set_row_data (prefs->table, row, (gpointer) account);
		}
#endif
		
		if (url)
			camel_url_free (url);
		
		node = node->next;
		row++;
	}
	
#if USE_ETABLE
	e_table_memory_thaw (E_TABLE_MEMORY (prefs->model));
#else
	gtk_clist_thaw (prefs->table);
#endif
}


#ifdef ENABLE_NNTP
static void
news_load (MailAccountsTab *prefs)
{
	const MailConfigService *service;
	const GSList *node;
	int i = 0;
	
	gtk_clist_freeze (prefs->news);
	
	gtk_clist_clear (prefs->news);
	
	node = mail_config_get_news ();
	
	while (node) {
		CamelURL *url;
		char *text[1];
		
	        service = node->data;
		
		if (service->url)
			url = camel_url_new (service->url, NULL);
		else
			url = NULL;
		
		text[0] = g_strdup_printf ("%s", url && url->host ? url->host : _("None"));
		
		if (url)
			camel_url_free (url);
		
		gtk_clist_append (prefs->news, text);
		g_free (text[0]);
		
		/* set the account on the row */
		gtk_clist_set_row_data (prefs->news, i, (gpointer) service);
		
		node = node->next;
		i++;
	}
	
	gtk_clist_thaw (prefs->news);
}


/* news callbacks */
static void
news_select_row (GtkCList *clist, int row, int column, GdkEventButton *event, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	
	prefs->news_row = row;
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->news_edit), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->news_delete), TRUE);
}

static void
news_unselect_row (GtkCList *clist, int row, int column, GdkEventButton *event, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	
	prefs->news_row = -1;
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->news_edit), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (prefs->news_delete), FALSE);
}

static void
news_editor_destroyed (GtkWidget *widget, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	
	news_load (prefs);
	prefs->news_editor = NULL;
}

static void
news_edit_clicked (GtkButton *button, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	
	if (prefs->news_editor == NULL) {
		if (prefs->news_row >= 0) {
			MailConfigService *service;
			
			service = gtk_clist_get_row_data (prefs->news, prefs->news_row);
			prefs->news_editor = mail_account_editor_news_new (service);
			gtk_signal_connect (GTK_OBJECT (prefs->news_editor), "destroy",
					    GTK_SIGNAL_FUNC (news_editor_destroyed),
					    prefs);
			gtk_widget_show (GTK_WIDGET (prefs->news_editor));
		}
	} else {
		gdk_window_raise (GTK_WIDGET (prefs->news_editor)->window);
	}
}

static void 
news_add_destroyed (GtkWidget *widget, gpointer user_data)
{
	gpointer *send = user_data;
	MailAccountsTab *prefs;
	MailConfigService *service;
	
	service = send[0];
	prefs = send[1];
	g_free (send);
	
	news_load (prefs);
	
	mail_load_storage_by_uri (prefs->shell, service->url, NULL);
	
	/* FIXME: why do we re-load? */
	news_load (prefs);
}

static void
news_add_clicked (GtkButton *button, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	MailConfigService *service;
	gpointer *send;
	
	if (prefs->news_editor == NULL) {
		send = g_new (gpointer, 2);
		
		service = g_new0 (MailConfigService, 1);
		service->url = NULL;
		
		prefs->news_editor = mail_account_editor_news_new (service);
		send[0] = service;
		send[1] = prefs;
		gtk_signal_connect (GTK_OBJECT (prefs->news_editor), "destroy",
				    GTK_SIGNAL_FUNC (news_add_destroyed),
				    send);
		gtk_widget_show (GTK_WIDGET (prefs->news_editor));
	} else {
		gdk_window_raise (GTK_WIDGET (prefs->news_editor)->window);
	}
}

static void
news_delete_clicked (GtkButton *button, gpointer user_data)
{
	MailAccountsTab *prefs = user_data;
	GtkWidget *window, *label;
	MailConfigService *server;
	GnomeDialog *confirm;
	int ans;
	
	/* don't allow user to delete an account if he might be editing it */
	if (prefs->news_row < 0 || prefs->news_editor != NULL)
		return;
	
	window = gtk_widget_get_ancestor (GTK_WIDGET (prefs), GTK_TYPE_WINDOW);
	
	confirm = GNOME_DIALOG (gnome_dialog_new (_("Are you sure you want to delete this news account?"),
						  GNOME_STOCK_BUTTON_YES, GNOME_STOCK_BUTTON_NO, NULL));
	gtk_window_set_policy (GTK_WINDOW (confirm), TRUE, TRUE, TRUE);
	gtk_window_set_modal (GTK_WINDOW (confirm), TRUE);
	label = gtk_label_new (_("Are you sure you want to delete this news account?"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (confirm->vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	gnome_dialog_set_parent (confirm, GTK_WINDOW (window));
	ans = gnome_dialog_run_and_close (confirm);
	
	if (ans == 0) {
		const GSList *servers;
		int row, len;
		
		server = gtk_clist_get_row_data (prefs->news, prefs->news_row);
		
		/* remove it from the folder-tree in the shell */
		if (server && server->url) {
			CamelProvider *prov;
			CamelException ex;
			
			camel_exception_init (&ex);
			prov = camel_session_get_provider (session, server->url, &ex);
			if (prov != NULL && prov->flags & CAMEL_PROVIDER_IS_STORAGE &&
			    prov->flags & CAMEL_PROVIDER_IS_REMOTE) {
				CamelService *store;
				
				store = camel_session_get_service (session, server->url,
								   CAMEL_PROVIDER_STORE, &ex);
				if (store != NULL) {
					g_warning ("removing news storage: %s", server->url);
					mail_remove_storage (CAMEL_STORE (store));
					camel_object_unref (CAMEL_OBJECT (store));
				}
			} else
				g_warning ("%s is not a remote news storage.", server->url);
			camel_exception_clear (&ex);
		}
		
		/* remove it from the config file */
		servers = mail_config_remove_news (server);
		mail_config_write ();
		
		gtk_clist_remove (prefs->news, prefs->news_row);
		
		len = servers ? g_slist_length ((GSList *) servers) : 0;
		if (len > 0) {
			row = prefs->news_row;
			row = row >= len ? len - 1 : row;
			gtk_clist_select_row (prefs->news, row, 0);
		} else {
			prefs->news_row = -1;
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->news_edit), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (prefs->news_delete), FALSE);
		}
	}
}
#endif /* ENABLE_NNTP */


GtkWidget *mail_accounts_etable_new (char *widget_name, char *string1, char *string2,
				     int int1, int int2);

#if USE_ETABLE
GtkWidget *
mail_accounts_etable_new (char *widget_name, char *string1, char *string2, int int1, int int2)
{
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
	
	images[0] = disabled_pixbuf;   /* disabled */
	images[1] = enabled_pixbuf;    /* enabled */
	e_table_extras_add_cell (extras, "render_able", e_cell_toggle_new (0, 2, images));
	
	model = e_table_memory_store_new (columns);
	
	return e_table_scrolled_new_from_spec_file (model, extras, EVOLUTION_ETSPECDIR "/mail-accounts.etspec", NULL);
}
#else
GtkWidget *
mail_accounts_etable_new (char *widget_name, char *string1, char *string2, int int1, int int2)
{
	GtkWidget *table, *scrolled;
	char *titles[3];
	
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	titles[0] = _("Enabled");
	titles[1] = _("Account name");
	titles[2] = _("Protocol");
	table = gtk_clist_new_with_titles (3, titles);
	gtk_clist_set_selection_mode (GTK_CLIST (table), GTK_SELECTION_SINGLE);
	gtk_clist_column_titles_show (GTK_CLIST (table));
	
	gtk_container_add (GTK_CONTAINER (scrolled), table);
	
	gtk_object_set_data (GTK_OBJECT (scrolled), "table", table);
	
	gtk_widget_show (scrolled);
	gtk_widget_show (table);
	
	return scrolled;
}
#endif

static void
mail_accounts_tab_construct (MailAccountsTab *prefs)
{
	GtkWidget *toplevel, *widget;
	GladeXML *gui;
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "accounts_tab");
	prefs->gui = gui;
	
	/* get our toplevel widget */
#ifdef ENABLE_NNTP
	toplevel = glade_xml_get_widget (gui, "toplevel_notebook");
#else
	toplevel = glade_xml_get_widget (gui, "toplevel");
#endif
	
	/* reparent */
	gtk_widget_ref (toplevel);
	gtk_container_remove (GTK_CONTAINER (toplevel->parent), toplevel);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
	gtk_widget_unref (toplevel);
	
	widget = glade_xml_get_widget (gui, "etableMailAccounts");
	
#if USE_ETABLE
	prefs->table = e_table_scrolled_get_table (E_TABLE_SCROLLED (widget));
	prefs->model = prefs->table->model;
	
	gtk_signal_connect (GTK_OBJECT (prefs->table), "cursor_change",
			    account_cursor_change, prefs);
	
	gtk_signal_connect (GTK_OBJECT (prefs->table), "double_click",
			    account_double_click, prefs);
	
	mail_accounts_load (prefs);
#else
	prefs->table = GTK_CLIST (gtk_object_get_data (GTK_OBJECT (widget), "table"));
	gtk_clist_set_column_justification (prefs->table, 0, GTK_JUSTIFY_RIGHT);
	
	gtk_signal_connect (GTK_OBJECT (prefs->table), "select-row",
			    account_cursor_change, prefs);
	
	mail_accounts_load (prefs);
	
	{
		int col;
		
		for (col = 0;  col < 3; col++) {
			gtk_clist_set_column_auto_resize (prefs->table, col, TRUE);
		}
	}
#endif
	
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
	
#ifdef ENABLE_NNTP
	prefs->news = GTK_CLIST (gtk_object_get_data (GTK_OBJECT (widget), "clistNews"));
	gtk_signal_connect (GTK_OBJECT (prefs->news), "select-row",
			    news_select_row, prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->news), "unselect-row",
			    news_unselect_row, prefs);
	
	news_load (prefs);
	
	prefs->news_add = GTK_BUTTON (glade_xml_get_widget (gui, "cmdNewsAdd"));
	gtk_signal_connect (GTK_OBJECT (prefs->news_add), "clicked",
			    news_add_clicked, prefs);
	
	prefs->news_edit = GTK_BUTTON (glade_xml_get_widget (gui, "cmdNewsEdit"));
	gtk_signal_connect (GTK_OBJECT (prefs->news_edit), "clicked",
			    news_edit_clicked, prefs);
	
	prefs->news_delete = GTK_BUTTON (glade_xml_get_widget (gui, "cmdNewsDelete"));
	gtk_signal_connect (GTK_OBJECT (prefs->news_delete), "clicked",
			    news_delete_clicked, prefs);
#endif /* ENABLE_NNTP */
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
