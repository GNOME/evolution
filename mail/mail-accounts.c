/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-messagebox.h>
#include <camel/camel-url.h>
#include <camel/camel-pgp-context.h>

#include <gal/widgets/e-unicode.h>

#include "widgets/misc/e-charset-picker.h"

#include "mail.h"
#include "mail-accounts.h"
#include "mail-config.h"
#include "mail-config-druid.h"
#include "mail-account-editor.h"
#ifdef ENABLE_NNTP
#include "mail-account-editor-news.h"
#endif
#include "mail-send-recv.h"
#include "mail-session.h"

#include "art/mark.xpm"

static void mail_accounts_dialog_class_init (MailAccountsDialogClass *class);
static void mail_accounts_dialog_init       (MailAccountsDialog *dialog);
static void mail_accounts_dialog_finalise   (GtkObject *obj);
static void mail_unselect                   (GtkCList *clist, int row, int column, GdkEventButton *event, gpointer data);
static void mail_able                       (GtkButton *button, gpointer data);

static MailConfigDruid *druid = NULL;
static MailAccountEditor *editor = NULL;
#ifdef ENABLE_NNTP
static MailAccountEditorNews *news_editor = NULL;
#endif

static GnomeDialogClass *parent_class;

GtkType
mail_accounts_dialog_get_type ()
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MailAccountsDialog",
			sizeof (MailAccountsDialog),
			sizeof (MailAccountsDialogClass),
			(GtkClassInitFunc) mail_accounts_dialog_class_init,
			(GtkObjectInitFunc) mail_accounts_dialog_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gnome_dialog_get_type (), &type_info);
	}
	
	return type;
}

static void
mail_accounts_dialog_class_init (MailAccountsDialogClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) class;
	parent_class = gtk_type_class (gnome_dialog_get_type ());
	
	object_class->finalize = mail_accounts_dialog_finalise;
	/* override methods */
	
}

static void
mail_accounts_dialog_init (MailAccountsDialog *o)
{
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) mark_xpm);
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &(o->mark_pixmap), &(o->mark_bitmap), 128);
	gdk_pixbuf_unref (pixbuf);
}

static void
mail_accounts_dialog_finalise (GtkObject *obj)
{
	MailAccountsDialog *dialog = (MailAccountsDialog *) obj;
	
	gtk_object_unref (GTK_OBJECT (dialog->gui));
	gdk_pixmap_unref (dialog->mark_pixmap);
	gdk_bitmap_unref (dialog->mark_bitmap);

        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

static void
load_accounts (MailAccountsDialog *dialog)
{
	const MailConfigAccount *account, *default_account;
	const GSList *node = dialog->accounts;
	int i = 0;
	
	gtk_clist_freeze (dialog->mail_accounts);
	
	gtk_clist_clear (dialog->mail_accounts);
	
	default_account = mail_config_get_default_account ();
	
	while (node) {
		CamelURL *url;
		char *text[3];
		
		account = node->data;
		
		if (account->source && account->source->url)
			url = camel_url_new (account->source->url, NULL);
		else
			url = NULL;
		
		text[0] = "";
		text[1] = e_utf8_to_gtk_string (GTK_WIDGET (dialog->mail_accounts), account->name);
		text[2] = g_strdup_printf ("%s%s", url && url->protocol ? url->protocol : _("None"),
					   (account == default_account) ? _(" (default)") : "");
		
		if (url)
			camel_url_free (url);
		
		gtk_clist_append (dialog->mail_accounts, text);
		g_free (text[1]);
		g_free (text[2]);
		
		if (account->source->enabled)
			gtk_clist_set_pixmap (dialog->mail_accounts, i, 0, 
					      dialog->mark_pixmap, 
					      dialog->mark_bitmap);
		
		/* set the account on the row */
		gtk_clist_set_row_data (dialog->mail_accounts, i, (gpointer) account);
		
		node = node->next;
		i++;
	}
	
	gtk_clist_thaw (dialog->mail_accounts);
	
	/* 
	 * The selection gets cleared when we rebuild the clist, but no
	 * unselect event is emitted.  So we simulate it here.
	 * I hate the clist.
	 */
	mail_unselect (dialog->mail_accounts, 0, 0, NULL, dialog);
}


/* mail callbacks */
static void
mail_select (GtkCList *clist, int row, int column, GdkEventButton *event, gpointer data)
{
	MailAccountsDialog *dialog = data;
	MailConfigAccount *account = gtk_clist_get_row_data (clist, row);
	
	dialog->accounts_row = row;
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_edit), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_delete), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_default), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_able), TRUE);
	if (account->source && account->source->enabled)
		gtk_label_set_text (GTK_LABEL (GTK_BIN (dialog->mail_able)->child), _("Disable"));
	else
		gtk_label_set_text (GTK_LABEL (GTK_BIN (dialog->mail_able)->child), _("Enable"));
	
	/* column 0 is the pixmap column */
	if (column == 0)
		mail_able (dialog->mail_able, data);
}

static void
mail_unselect (GtkCList *clist, int row, int column, GdkEventButton *event, gpointer data)
{
	MailAccountsDialog *dialog = data;
	
	dialog->accounts_row = -1;
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_edit), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_delete), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_default), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_able), FALSE);
	
	/* column 0 is the pixmap column */
	if (column == 0)
		mail_able (dialog->mail_able, data);
	
	/*
	 * If an insensitive button in a button box has the focus, and if you hit tab,
	 * there is a segfault.  I think that this might be a gtk bug.  Anyway, this
	 * is a workaround.
	 */
	gtk_widget_grab_focus (GTK_WIDGET (dialog->mail_add));
}

static void
mail_add_finished (GtkWidget *widget, gpointer data)
{
	/* Either Cancel or Finished was clicked in the druid so reload the accounts */
	MailAccountsDialog *dialog = data;
	
	dialog->accounts = mail_config_get_accounts ();
	load_accounts (dialog);
	druid = NULL;
}

static void
mail_add (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	
	if (druid == NULL) {
		druid = mail_config_druid_new (dialog->shell);
		gtk_signal_connect (GTK_OBJECT (druid), "destroy",
				    GTK_SIGNAL_FUNC (mail_add_finished), dialog);
		
		gtk_widget_show (GTK_WIDGET (druid));
	} else {
		gdk_window_raise (GTK_WIDGET (druid)->window);
	}
}

static void
mail_editor_destroyed (GtkWidget *widget, gpointer data)
{
	load_accounts (MAIL_ACCOUNTS_DIALOG (data));
	editor = NULL;
}

static void
mail_edit (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	
	if (editor == NULL) {
		if (dialog->accounts_row >= 0) {
			MailConfigAccount *account;
			
			account = gtk_clist_get_row_data (dialog->mail_accounts, dialog->accounts_row);
			editor = mail_account_editor_new (account, GTK_WINDOW (dialog));
			gtk_signal_connect (GTK_OBJECT (editor), "destroy",
					    GTK_SIGNAL_FUNC (mail_editor_destroyed),
					    dialog);
			gtk_widget_show (GTK_WIDGET (editor));
		}
	} else {
		gdk_window_raise (GTK_WIDGET (editor)->window);
	}
}

static void
mail_double_click (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->type == GDK_2BUTTON_PRESS)
		mail_edit (NULL, data);
}

static void
mail_delete (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	MailConfigAccount *account;
	GnomeDialog *confirm;
	int ans;
	
	/* make sure we have a valid account selected and that we aren't editing anything... */
	if (dialog->accounts_row < 0 || editor != NULL)
		return;
	
	confirm = GNOME_DIALOG (gnome_message_box_new (_("Are you sure you want to delete this account?"),
						       GNOME_MESSAGE_BOX_QUESTION,
						       NULL));
	gnome_dialog_append_button_with_pixmap (confirm, _("Delete"), GNOME_STOCK_BUTTON_YES);
	gnome_dialog_append_button_with_pixmap (confirm, _("Don't delete"), GNOME_STOCK_BUTTON_NO);
	gtk_window_set_policy (GTK_WINDOW (confirm), TRUE, TRUE, TRUE);
	gtk_window_set_modal (GTK_WINDOW (confirm), TRUE);
	gtk_window_set_title (GTK_WINDOW (confirm), _("Really delete account?"));
	gnome_dialog_set_parent (confirm, GTK_WINDOW (dialog));
	ans = gnome_dialog_run_and_close (confirm);
	
	if (ans == 0) {
		int sel, row, len;
		
		sel = dialog->accounts_row;
		
		account = gtk_clist_get_row_data (dialog->mail_accounts, sel);
		
		/* remove it from the folder-tree in the shell */
		if (account->source && account->source->url && account->source->enabled)
			mail_remove_storage_by_uri (account->source->url);
		
		/* remove it from the config file */
		dialog->accounts = mail_config_remove_account (account);
		mail_config_write ();
		mail_autoreceive_setup ();
		
		gtk_clist_remove (dialog->mail_accounts, sel);
		
		len = dialog->accounts ? g_slist_length ((GSList *) dialog->accounts) : 0;
		if (len > 0) {
			row = sel >= len ? len - 1 : sel;
			load_accounts (dialog);
			gtk_clist_select_row (dialog->mail_accounts, row, 1);
		} else {
			dialog->accounts_row = -1;
			gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_edit), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_delete), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_default), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_able), FALSE);
		}
	}
}

static void
mail_default (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	const MailConfigAccount *account;
	
	if (dialog->accounts_row >= 0) {
		int row;
		
		row = dialog->accounts_row;
		account = gtk_clist_get_row_data (dialog->mail_accounts, row);
		mail_config_set_default_account (account);
		mail_config_write ();
		load_accounts (dialog);
		gtk_clist_select_row (dialog->mail_accounts, row, 1);
	}
}

static void
mail_able (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	const MailConfigAccount *account;
	
	if (dialog->accounts_row >= 0) {
		int row;
		
		row = dialog->accounts_row;
		account = gtk_clist_get_row_data (dialog->mail_accounts, row);
		account->source->enabled = !account->source->enabled;
		
		if (account->source && account->source->url) {
			if (account->source->enabled)
				mail_load_storage_by_uri (dialog->shell, account->source->url, account->name);
			else
				mail_remove_storage_by_uri (account->source->url);
		}
		
		mail_autoreceive_setup ();
		mail_config_write ();
		load_accounts (dialog);
		gtk_clist_select_row (dialog->mail_accounts, row, 1);
	}
}


#ifdef ENABLE_NNTP
static void
load_news (MailAccountsDialog *dialog)
{
	const MailConfigService *service;
	const GSList *node = dialog->news;
	int i = 0;

	gtk_clist_freeze (dialog->news_accounts);
	
	gtk_clist_clear (dialog->news_accounts);
	
	while (node) {
		CamelURL *url;
		gchar *text[1];
		
	        service = node->data;
		
		if (service->url)
			url = camel_url_new (service->url, NULL);
		else
			url = NULL;
		
		text[0] = g_strdup_printf ("%s", url && url->host ? url->host : _("None"));
		
		if (url)
			camel_url_free (url);
		
		gtk_clist_append (dialog->news_accounts, text);
		g_free (text[0]);
		
		/* set the account on the row */
		gtk_clist_set_row_data (dialog->news_accounts, i, (gpointer) service);
		
		node = node->next;
		i++;
	}
	
	gtk_clist_thaw (dialog->news_accounts);
}


/* news callbacks */
static void
news_select (GtkCList *clist, gint row, gint column, GdkEventButton *event, gpointer data)
{
	MailAccountsDialog *dialog = data;
	
	dialog->news_row = row;
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->news_edit), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->news_delete), TRUE);
}

static void
news_unselect (GtkCList *clist, gint row, gint column, GdkEventButton *event, gpointer data)
{
	MailAccountsDialog *dialog = data;
	
	dialog->news_row = -1;
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->news_edit), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->news_delete), FALSE);
}

static void
news_editor_destroyed (GtkWidget *widget, gpointer data)
{
	load_news (MAIL_ACCOUNTS_DIALOG (data));
	news_editor = NULL;
}

static void
news_edit (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	
	if (news_editor == NULL) {
		if (dialog->news_row >= 0) {
			MailConfigService *service;
			
			service = gtk_clist_get_row_data (dialog->news_accounts, dialog->news_row);
			news_editor = mail_account_editor_news_new (service);
			gtk_signal_connect (GTK_OBJECT (news_editor), "destroy",
					    GTK_SIGNAL_FUNC (news_editor_destroyed),
					    dialog);
			gtk_widget_show (GTK_WIDGET (news_editor));
		}
	} else {
		gdk_window_raise (GTK_WIDGET (news_editor)->window);
	}
}

static void 
news_add_destroyed (GtkWidget *widget, gpointer data)
{
	gpointer *send = data;
	MailAccountsDialog *dialog;
	MailConfigService *service;

	service = send[0];
	dialog = send[1];
	g_free(send);

	dialog->news = mail_config_get_news ();
	load_news (dialog);

	mail_load_storage_by_uri(dialog->shell, service->url, NULL);
	
	dialog->news = mail_config_get_news ();
	load_news (dialog);
	
}

static void
news_add (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	MailConfigService *service;
	gpointer *send;
	
	if (news_editor == NULL) {
		send = g_new (gpointer, 2);
		
		service = g_new0 (MailConfigService, 1);
		service->url = NULL;
		
		news_editor = mail_account_editor_news_new (service);
		send[0] = service;
		send[1] = dialog;
		gtk_signal_connect (GTK_OBJECT (news_editor), "destroy",
				    GTK_SIGNAL_FUNC (news_add_destroyed),
				    send);
		gtk_widget_show (GTK_WIDGET (news_editor));
	} else {
		gdk_window_raise (GTK_WIDGET (news_editor)->window);
	}
}

static void
news_delete (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	MailConfigService *server;
	GnomeDialog *confirm;
	GtkWidget *label;
	int ans;
	
	/* don't allow user to delete an account if he might be editing it */
	if (dialog->news_row < 0 || news_editor != NULL)
		return;
	
	confirm = GNOME_DIALOG (gnome_dialog_new (_("Are you sure you want to delete this news account?"),
						  GNOME_STOCK_BUTTON_YES, GNOME_STOCK_BUTTON_NO, NULL));
	gtk_window_set_policy (GTK_WINDOW (confirm), TRUE, TRUE, TRUE);
	gtk_window_set_modal (GTK_WINDOW (confirm), TRUE);
	label = gtk_label_new (_("Are you sure you want to delete this news account?"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (confirm->vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	gnome_dialog_set_parent (confirm, GTK_WINDOW (dialog));
	ans = gnome_dialog_run_and_close (confirm);
	
	if (ans == 0) {
		int row, len;
		
		server = gtk_clist_get_row_data (dialog->news_accounts, dialog->news_row);
		
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
		dialog->news = mail_config_remove_news (server);
		mail_config_write ();
		
		gtk_clist_remove (dialog->news_accounts, dialog->news_row);
		
		len = dialog->news ? g_slist_length ((GSList *) dialog->news) : 0;
		if (len > 0) {
			row = dialog->news_row;
			row = row >= len ? len - 1 : row;
			gtk_clist_select_row (dialog->news_accounts, row, 0);
		} else {
			dialog->news_row = -1;
			gtk_widget_set_sensitive (GTK_WIDGET (dialog->news_edit), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (dialog->news_delete), FALSE);
		}
	}
}
#endif /* ENABLE_NNTP */

/* temp widget callbacks */
static void
send_html_toggled (GtkToggleButton *button, gpointer data)
{
	mail_config_set_send_html (gtk_toggle_button_get_active (button));
}

static void
citation_highlight_toggled (GtkToggleButton *button, gpointer data)
{
	mail_config_set_citation_highlight (gtk_toggle_button_get_active (button));
}

static void
timeout_toggled (GtkToggleButton *button, gpointer data)
{
	mail_config_set_do_seen_timeout (gtk_toggle_button_get_active (button));
}

static void
citation_color_set (GnomeColorPicker *cp, guint r, guint g, guint b, guint a)
{
	guint32 rgb;

	rgb   = r >> 8;
	rgb <<= 8;
	rgb  |= g >> 8;
	rgb <<= 8;
	rgb  |= b >> 8;

	mail_config_set_citation_color (rgb);
}

/* FIXME: */

static void
timeout_changed (GtkEntry *entry, gpointer data)
{
	MailAccountsDialog *dialog = data;
	gint val;
	
	val = (gint) (gtk_spin_button_get_value_as_float (dialog->timeout) * 1000);
	
	mail_config_set_mark_as_seen_timeout (val);
}

static void
pgp_path_changed (GtkEntry *entry, gpointer data)
{
	CamelPgpType type;
	const char *path;
	
	path = gtk_entry_get_text (entry);
	
	type = mail_config_pgp_type_detect_from_path (path);
	
	mail_config_set_pgp_path (path && *path ? path : NULL);
	mail_config_set_pgp_type (type);
}

static void
filter_log_path_changed (GtkEntry *entry, gpointer data)
{
	const char *path;
	
	path = gtk_entry_get_text (entry);
	
	mail_config_set_filter_log_path (path && *path ? path : NULL);
}

static void
set_color (GnomeColorPicker *cp)
{
	guint32 rgb = mail_config_get_citation_color ();

	gnome_color_picker_set_i8 (cp, (rgb & 0xff0000) >> 16, (rgb & 0xff00) >> 8, rgb & 0xff, 0xff);
}

static void
images_radio_toggled (GtkWidget *radio, gpointer data)
{
	MailAccountsDialog *dialog = data;
	
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio)))
		return;
	
	if (radio == (GtkWidget *)dialog->images_always)
		mail_config_set_http_mode (MAIL_CONFIG_HTTP_ALWAYS);
	else if (radio == (GtkWidget *)dialog->images_sometimes)
		mail_config_set_http_mode (MAIL_CONFIG_HTTP_SOMETIMES);
	else
		mail_config_set_http_mode (MAIL_CONFIG_HTTP_NEVER);
}

static void
notify_radio_toggled (GtkWidget *radio, gpointer data)
{
	MailAccountsDialog *dialog = data;
	
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio)))
		return;
	
	if (radio == (GtkWidget *) dialog->notify_not)
		mail_config_set_new_mail_notify (MAIL_CONFIG_NOTIFY_NOT);
	else if (radio == (GtkWidget *) dialog->notify_beep)
		mail_config_set_new_mail_notify (MAIL_CONFIG_NOTIFY_BEEP);
	else
		mail_config_set_new_mail_notify (MAIL_CONFIG_NOTIFY_PLAY_SOUND);
}

static void
notify_sound_file_changed (GtkWidget *entry, gpointer data)
{
	char *filename;
	
	filename = gtk_entry_get_text (GTK_ENTRY (entry));
	mail_config_set_new_mail_notify_sound_file (filename);
}

static void
empty_trash_toggled (GtkWidget *toggle, gpointer data)
{
	mail_config_set_empty_trash_on_exit (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}

static void
prompt_empty_subject_toggled (GtkWidget *toggle, gpointer data)
{
	mail_config_set_prompt_empty_subject (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}

static void
prompt_bcc_only_toggled (GtkWidget *toggle, gpointer data)
{
	mail_config_set_prompt_only_bcc (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}

static void
prompt_unwanted_html_toggled (GtkWidget *toggle, gpointer data)
{
	mail_config_set_confirm_unwanted_html (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}

#if 0
/* Note: Please see construct() for a reason as to why these 2 options are disabled */
static void
thread_list_toggled (GtkWidget *toggle, gpointer data)
{
	mail_config_set_thread_list (NULL, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}

static void
show_preview_toggled (GtkWidget *toggle, gpointer data)
{
	mail_config_set_show_preview (NULL, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}
#endif

static void
filter_log_toggled (GtkWidget *toggle, gpointer data)
{
	mail_config_set_filter_log (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}

static void
confirm_expunge_toggled (GtkWidget *toggle, gpointer data)
{
	mail_config_set_confirm_expunge (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}

static void
forward_style_activated (GtkWidget *item, gpointer data)
{
	int style = GPOINTER_TO_INT (data);

	mail_config_set_default_forward_style (style);
}

static void
attach_forward_style_signal (GtkWidget *item, gpointer data)
{
	int *num = data;

	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    forward_style_activated, GINT_TO_POINTER (*num));
	(*num)++;
}

static void
charset_menu_deactivate (GtkWidget *menu, gpointer data)
{
	char *charset;

	charset = e_charset_picker_get_charset (menu);
	if (charset) {
		mail_config_set_default_charset (charset);
		g_free (charset);
	}
}

static void
dialog_destroy (GtkWidget *dialog, gpointer user_data)
{
	if (druid)
		gtk_widget_destroy (GTK_WIDGET (druid));
	
	if (editor)
		gtk_widget_destroy (GTK_WIDGET (editor));
	
#ifdef ENABLE_NNTP
	if (news_editor)
		gtk_widget_destroy (GTK_WIDGET (news_editor));
#endif
}

static void
construct (MailAccountsDialog *dialog)
{
	GladeXML *gui;
	GtkWidget *notebook, *menu;
	const char *text;
	int num;
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", NULL);
	dialog->gui = gui;
	
	/* get our toplevel widget */
	notebook = glade_xml_get_widget (gui, "notebook");
	
	/* reparent */
	gtk_widget_reparent (notebook, GNOME_DIALOG (dialog)->vbox);
	
	/* give our dialog an Close button and title */
	gtk_window_set_title (GTK_WINDOW (dialog), _("Mail Settings"));
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, TRUE);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 300);
	gnome_dialog_append_button (GNOME_DIALOG (dialog), GNOME_STOCK_BUTTON_OK);
	
	gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
			    GTK_SIGNAL_FUNC (dialog_destroy), dialog);
	
	dialog->mail_accounts = GTK_CLIST (glade_xml_get_widget (gui, "clistAccounts"));
	gtk_signal_connect (GTK_OBJECT (dialog->mail_accounts), "select-row",
			    GTK_SIGNAL_FUNC (mail_select), dialog);
	gtk_signal_connect (GTK_OBJECT (dialog->mail_accounts), "unselect-row",
			    GTK_SIGNAL_FUNC (mail_unselect), dialog);
	gtk_signal_connect (GTK_OBJECT (dialog->mail_accounts), "button_press_event",
			    mail_double_click, dialog);
	dialog->mail_add = GTK_BUTTON (glade_xml_get_widget (gui, "cmdMailAdd"));
	gtk_signal_connect (GTK_OBJECT (dialog->mail_add), "clicked",
			    GTK_SIGNAL_FUNC (mail_add), dialog);
	dialog->mail_edit = GTK_BUTTON (glade_xml_get_widget (gui, "cmdMailEdit"));
	gtk_signal_connect (GTK_OBJECT (dialog->mail_edit), "clicked",
			    GTK_SIGNAL_FUNC (mail_edit), dialog);
	dialog->mail_delete = GTK_BUTTON (glade_xml_get_widget (gui, "cmdMailDelete"));
	gtk_signal_connect (GTK_OBJECT (dialog->mail_delete), "clicked",
			    GTK_SIGNAL_FUNC (mail_delete), dialog);
	dialog->mail_default = GTK_BUTTON (glade_xml_get_widget (gui, "cmdMailDefault"));
	gtk_signal_connect (GTK_OBJECT (dialog->mail_default), "clicked",
			    GTK_SIGNAL_FUNC (mail_default), dialog);
	dialog->mail_able = GTK_BUTTON (glade_xml_get_widget (gui, "cmdMailAble"));
	gtk_signal_connect (GTK_OBJECT (dialog->mail_able), "clicked",
			    GTK_SIGNAL_FUNC (mail_able), dialog);
	
#ifdef ENABLE_NNTP
	dialog->news_accounts = GTK_CLIST (glade_xml_get_widget (gui, "clistNews"));
	gtk_signal_connect (GTK_OBJECT (dialog->news_accounts), "select-row",
			    GTK_SIGNAL_FUNC (news_select), dialog);
	gtk_signal_connect (GTK_OBJECT (dialog->news_accounts), "unselect-row",
			    GTK_SIGNAL_FUNC (news_unselect), dialog);
	dialog->news_add = GTK_BUTTON (glade_xml_get_widget (gui, "cmdNewsAdd"));
	gtk_signal_connect (GTK_OBJECT (dialog->news_add), "clicked",
			    GTK_SIGNAL_FUNC (news_add), dialog);
	dialog->news_edit = GTK_BUTTON (glade_xml_get_widget (gui, "cmdNewsEdit"));
	gtk_signal_connect (GTK_OBJECT (dialog->news_edit), "clicked",
			    GTK_SIGNAL_FUNC (news_edit), dialog);
	dialog->news_delete = GTK_BUTTON (glade_xml_get_widget (gui, "cmdNewsDelete"));
	gtk_signal_connect (GTK_OBJECT (dialog->news_delete), "clicked",
			    GTK_SIGNAL_FUNC (news_delete), dialog);
#else
	/* remove the news tab since we don't support nntp */
	gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), 1);
#endif
	
	/* Display page */
	dialog->citation_highlight = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chckHighlightCitations"));
	gtk_toggle_button_set_active (dialog->citation_highlight, mail_config_get_citation_highlight ());
	gtk_signal_connect (GTK_OBJECT (dialog->citation_highlight), "toggled",
			    GTK_SIGNAL_FUNC (citation_highlight_toggled), dialog);
	dialog->citation_color = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, "colorpickerCitations"));
	set_color (dialog->citation_color);
	gtk_signal_connect (GTK_OBJECT (dialog->citation_color), "color_set",
			    GTK_SIGNAL_FUNC (citation_color_set), dialog);
	
	dialog->timeout_toggle = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "checkMarkTimeout"));
	gtk_toggle_button_set_active (dialog->timeout_toggle, mail_config_get_do_seen_timeout ());
	gtk_signal_connect (GTK_OBJECT (dialog->timeout_toggle), "toggled",
			    GTK_SIGNAL_FUNC (timeout_toggled), dialog);
	
	dialog->timeout = GTK_SPIN_BUTTON (glade_xml_get_widget (gui, "spinMarkTimeout"));
	gtk_spin_button_set_value (dialog->timeout, (1.0 * mail_config_get_mark_as_seen_timeout ()) / 1000.0);
	gtk_signal_connect (GTK_OBJECT (dialog->timeout), "changed",
			    GTK_SIGNAL_FUNC (timeout_changed), dialog);
	
	dialog->images_never = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radioImagesNever"));
	gtk_toggle_button_set_active (dialog->images_never, mail_config_get_http_mode () == MAIL_CONFIG_HTTP_NEVER);
	gtk_signal_connect (GTK_OBJECT (dialog->images_never), "toggled",
			    GTK_SIGNAL_FUNC (images_radio_toggled), dialog);
	dialog->images_sometimes = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radioImagesSometimes"));
	gtk_toggle_button_set_active (dialog->images_sometimes, mail_config_get_http_mode () == MAIL_CONFIG_HTTP_SOMETIMES);
	gtk_signal_connect (GTK_OBJECT (dialog->images_sometimes), "toggled",
			    GTK_SIGNAL_FUNC (images_radio_toggled), dialog);
	dialog->images_always = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radioImagesAlways"));
	gtk_toggle_button_set_active (dialog->images_always, mail_config_get_http_mode () == MAIL_CONFIG_HTTP_ALWAYS);
	gtk_signal_connect (GTK_OBJECT (dialog->images_always), "toggled",
			    GTK_SIGNAL_FUNC (images_radio_toggled), dialog);
	
#if 0
	/* These options are disabled because they are completely non-intuitive and evil */
	dialog->thread_list = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkThreadedList"));
	gtk_toggle_button_set_active (dialog->thread_list, mail_config_get_thread_list (NULL));
	gtk_signal_connect (GTK_OBJECT (dialog->thread_list), "toggled",
			    GTK_SIGNAL_FUNC (thread_list_toggled), dialog);
	
	dialog->show_preview = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkShowPreview"));
	gtk_toggle_button_set_active (dialog->show_preview, mail_config_get_show_preview (NULL));
	gtk_signal_connect (GTK_OBJECT (dialog->show_preview), "toggled",
			    GTK_SIGNAL_FUNC (show_preview_toggled), dialog);
#endif
	
	/* Composer page */
	dialog->send_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkSendHTML"));
	gtk_toggle_button_set_active (dialog->send_html, mail_config_get_send_html ());
	gtk_signal_connect (GTK_OBJECT (dialog->send_html), "toggled",
			    GTK_SIGNAL_FUNC (send_html_toggled), dialog);
	
	dialog->forward_style = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuForwardStyle"));
	gtk_option_menu_set_history (dialog->forward_style, mail_config_get_default_forward_style ());
	/* Hm. This sucks... */
	num = 0;
	gtk_container_foreach (GTK_CONTAINER (gtk_option_menu_get_menu (dialog->forward_style)),
			       attach_forward_style_signal, &num);
	
	dialog->prompt_empty_subject = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptEmptySubject"));
	gtk_toggle_button_set_active (dialog->prompt_empty_subject, mail_config_get_prompt_empty_subject ());
	gtk_signal_connect (GTK_OBJECT (dialog->prompt_empty_subject), "toggled",
			    GTK_SIGNAL_FUNC (prompt_empty_subject_toggled), dialog);
	
	dialog->prompt_bcc_only = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptBccOnly"));
	gtk_toggle_button_set_active (dialog->prompt_bcc_only, mail_config_get_prompt_only_bcc ());
	gtk_signal_connect (GTK_OBJECT (dialog->prompt_bcc_only), "toggled",
			    GTK_SIGNAL_FUNC (prompt_bcc_only_toggled), dialog);
	
	dialog->prompt_unwanted_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptWantHTML"));
	gtk_toggle_button_set_active (dialog->prompt_unwanted_html, mail_config_get_confirm_unwanted_html ());
	gtk_signal_connect (GTK_OBJECT (dialog->prompt_unwanted_html), "toggled",
			    GTK_SIGNAL_FUNC (prompt_unwanted_html_toggled), dialog);
	
	/* Other page */
	dialog->pgp_path = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "filePgpPath"));
	text = mail_config_get_pgp_path ();
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (dialog->pgp_path)),
			    text ? text : "");
	gnome_file_entry_set_default_path (dialog->pgp_path, mail_config_get_pgp_path ());
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (dialog->pgp_path)),
			    "changed", GTK_SIGNAL_FUNC (pgp_path_changed), dialog);
	
	dialog->charset = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuCharset"));
	menu = e_charset_picker_new (mail_config_get_default_charset ());
	gtk_option_menu_set_menu (dialog->charset, GTK_WIDGET (menu));
	gtk_signal_connect (GTK_OBJECT (menu), "deactivate",
			    GTK_SIGNAL_FUNC (charset_menu_deactivate), NULL);
	
	dialog->empty_trash = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkEmptyTrashOnExit"));
	gtk_toggle_button_set_active (dialog->empty_trash, mail_config_get_empty_trash_on_exit ());
	gtk_signal_connect (GTK_OBJECT (dialog->empty_trash), "toggled",
			    GTK_SIGNAL_FUNC (empty_trash_toggled), dialog);
	
	dialog->filter_log = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkFilterLog"));
	gtk_toggle_button_set_active (dialog->filter_log, mail_config_get_filter_log ());
	gtk_signal_connect (GTK_OBJECT (dialog->filter_log), "toggled",
			    GTK_SIGNAL_FUNC (filter_log_toggled), dialog);
	
	dialog->filter_log_path = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileFilterLog"));
	text = mail_config_get_filter_log_path ();
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (dialog->filter_log_path)),
			    text ? text : "");
	gnome_file_entry_set_default_path (dialog->filter_log_path, mail_config_get_filter_log_path ());
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (dialog->filter_log_path)),
			    "changed", GTK_SIGNAL_FUNC (filter_log_path_changed), dialog);
	
	dialog->confirm_expunge = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkConfirmExpunge"));
	gtk_toggle_button_set_active (dialog->confirm_expunge, mail_config_get_confirm_expunge ());
	gtk_signal_connect (GTK_OBJECT (dialog->confirm_expunge), "toggled",
			    GTK_SIGNAL_FUNC (confirm_expunge_toggled), dialog);
	
	dialog->notify_not = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radioNotifyNot"));
	gtk_toggle_button_set_active (dialog->notify_not, mail_config_get_new_mail_notify () == MAIL_CONFIG_NOTIFY_NOT);
	gtk_signal_connect (GTK_OBJECT (dialog->notify_not), "toggled",
			    GTK_SIGNAL_FUNC (notify_radio_toggled), dialog);
	
	dialog->notify_beep = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radioNotifyBeep"));
	gtk_toggle_button_set_active (dialog->notify_beep, mail_config_get_new_mail_notify () == MAIL_CONFIG_NOTIFY_BEEP);
	gtk_signal_connect (GTK_OBJECT (dialog->notify_beep), "toggled",
			    GTK_SIGNAL_FUNC (notify_radio_toggled), dialog);
	
	dialog->notify_play_sound = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "radioNotifyPlaySound"));
	gtk_toggle_button_set_active (dialog->notify_play_sound,
				      mail_config_get_new_mail_notify () == MAIL_CONFIG_NOTIFY_PLAY_SOUND);
	gtk_signal_connect (GTK_OBJECT (dialog->notify_play_sound), "toggled",
			    GTK_SIGNAL_FUNC (notify_radio_toggled), dialog);
	
	dialog->notify_sound_file = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileNotifyPlaySound"));
	text = mail_config_get_new_mail_notify_sound_file ();
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (dialog->notify_sound_file)),
			    text ? text : "");
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (dialog->notify_sound_file)),
			    "changed", GTK_SIGNAL_FUNC (notify_sound_file_changed), dialog);
	
	/* now to fill in the clists */
	dialog->accounts_row = -1;
	dialog->accounts = mail_config_get_accounts ();
	if (dialog->accounts) {
		load_accounts (dialog);
		gtk_clist_select_row (dialog->mail_accounts, 0, 1);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_edit), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_delete), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_default), FALSE);
	}
	
#ifdef ENABLE_NNTP
	dialog->news_row = -1;
	dialog->news = mail_config_get_news ();
	if (dialog->news) {
		load_news (dialog);
		gtk_clist_select_row (dialog->news_accounts, 0, 0);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->news_edit), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->news_delete), FALSE);
	}
#endif /* ENABLE_NNTP */
}

MailAccountsDialog *
mail_accounts_dialog_new (GNOME_Evolution_Shell shell)
{
	MailAccountsDialog *new;
	
	new = (MailAccountsDialog *) gtk_type_new (mail_accounts_dialog_get_type ());
	construct (new);
	new->shell = shell;
	
	return new;
}
