/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2001 Helix Code, Inc. (www.helixcode.com)
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

#include "config.h"

#include "mail-accounts.h"
#include "mail-config.h"
#include "mail-config-druid.h"
#include "mail-account-editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <camel/camel-url.h>
#include <openpgp-utils.h>

static void mail_accounts_dialog_class_init (MailAccountsDialogClass *class);
static void mail_accounts_dialog_init       (MailAccountsDialog *dialog);
static void mail_accounts_dialog_finalise   (GtkObject *obj);

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
	;
}

static void
mail_accounts_dialog_finalise (GtkObject *obj)
{
	MailAccountsDialog *dialog = (MailAccountsDialog *) obj;
	
	gtk_object_unref (GTK_OBJECT (dialog->gui));
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

static void
load_accounts (MailAccountsDialog *dialog)
{
	const MailConfigAccount *account;
	const GSList *node = dialog->accounts;
	int i = 0;
	
	gtk_clist_freeze (dialog->mail_accounts);
	
	gtk_clist_clear (dialog->mail_accounts);
	
	while (node) {
		CamelURL *url;
		gchar *text[2];
		
		account = node->data;
		
		if (account->source->url)
			url = camel_url_new (account->source->url, NULL);
		else
			url = NULL;
		
		text[0] = g_strdup (account->name);
		text[1] = g_strdup_printf ("%s%s", url && url->protocol ? url->protocol : _("None"),
					   account->default_account ? _(" (default)") : "");
		
		if (url)
			camel_url_free (url);
		
		gtk_clist_append (dialog->mail_accounts, text);
		g_free (text[0]);
		g_free (text[1]);
		
		/* set the account on the row */
		gtk_clist_set_row_data (dialog->mail_accounts, i, (gpointer) account);
		
		node = node->next;
		i++;
	}
	
	gtk_clist_thaw (dialog->mail_accounts);
}

static void
load_news (MailAccountsDialog *dialog)
{
	/* FIXME: implement */
	;
}

/* mail callbacks */
static void
mail_select (GtkCList *clist, gint row, gint column, GdkEventButton *event, gpointer data)
{
	MailAccountsDialog *dialog = data;
	
	dialog->accounts_row = row;
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_edit), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_delete), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_default), TRUE);
}

static void
mail_unselect (GtkCList *clist, gint row, gint column, GdkEventButton *event, gpointer data)
{
	MailAccountsDialog *dialog = data;
	
	dialog->accounts_row = -1;
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_edit), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_delete), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_default), FALSE);
}

static void
mail_add_finished (GtkWidget *widget, gpointer data)
{
	/* Either Cancel or Finished was clicked in the druid so reload the accounts */
	MailAccountsDialog *dialog = data;
	
	dialog->accounts = mail_config_get_accounts ();
	load_accounts (dialog);
}

static void
mail_add (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	MailConfigDruid *druid;
	
	druid = mail_config_druid_new (dialog->shell);
	gtk_signal_connect (GTK_OBJECT (druid), "destroy",
			    GTK_SIGNAL_FUNC (mail_add_finished), dialog);
	
	gtk_widget_show (GTK_WIDGET (druid));
}

static void
mail_editor_destroyed (GtkWidget *widget, gpointer data)
{
	load_accounts (MAIL_ACCOUNTS_DIALOG (data));
}

static void
mail_edit (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	
	if (dialog->accounts_row >= 0) {
		const MailConfigAccount *account;
		MailAccountEditor *editor;
		
		account = gtk_clist_get_row_data (dialog->mail_accounts, dialog->accounts_row);
		editor = mail_account_editor_new (account);
		gtk_signal_connect (GTK_OBJECT (editor), "destroy",
				    GTK_SIGNAL_FUNC (mail_editor_destroyed),
				    dialog);
		gtk_widget_show (GTK_WIDGET (editor));
	}
}

static void
mail_delete (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	MailConfigAccount *account;
	GnomeDialog *confirm;
	GtkWidget *label;
	int ans;
	
	if (dialog->accounts_row < 0)
		return;
	
	confirm = GNOME_DIALOG (gnome_dialog_new (_("Are you sure you want to delete this account?"),
						  GNOME_STOCK_BUTTON_YES, GNOME_STOCK_BUTTON_NO, NULL));
	gtk_window_set_policy (GTK_WINDOW (confirm), TRUE, TRUE, TRUE);
	gtk_window_set_modal (GTK_WINDOW (confirm), TRUE);
	label = gtk_label_new (_("Are you sure you want to delete this account?"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (confirm->vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	gnome_dialog_set_parent (confirm, GTK_WINDOW (dialog));
	ans = gnome_dialog_run_and_close (confirm);
	
	if (ans == 0) {
		int sel, row, len;
		
		sel = dialog->accounts_row;
		
		account = gtk_clist_get_row_data (dialog->mail_accounts, sel);
		dialog->accounts = mail_config_remove_account (account);
		mail_config_write ();
		
		gtk_clist_remove (dialog->mail_accounts, sel);
		
		len = dialog->accounts ? g_slist_length ((GSList *) dialog->accounts) : 0;
		if (len > 0) {
			row = sel >= len ? len - 1 : sel;
			gtk_clist_select_row (dialog->mail_accounts, row, 0);
		} else {
			dialog->accounts_row = -1;
			gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_edit), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_delete), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (dialog->mail_default), FALSE);
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
		gtk_clist_select_row (dialog->mail_accounts, row, 0);
	}
}

#ifdef ENABLE_NNTP
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
news_add_finish_clicked ()
{
	/* FIXME: uhm, yea... */
	;
}

static void
news_add (GtkButton *button, gpointer data)
{
	/* FIXME: do stuff */
	;
}

static void
news_edit (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	MailConfigService *server;
	
	/* FIXME: open the editor and stuff */
}

static void
news_delete (GtkButton *button, gpointer data)
{
	MailAccountsDialog *dialog = data;
	MailConfigService *server;
	GnomeDialog *confirm;
	GtkWidget *label;
	int ans;
	
	if (dialog->news_row < 0)
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
	const char *path, *bin;
	PgpType type = PGP_TYPE_NONE;
	
	path = gtk_entry_get_text (entry);
	bin = g_basename (path);
	
	/* FIXME: This detection should be better */
	if (!strcmp (bin, "pgp"))
		type = PGP_TYPE_PGP2;
	else if (!strcmp (bin, "pgpv") || !strcmp (bin, "pgpe") || !strcmp (bin, "pgpk") || !strcmp (bin, "pgps"))
		type = PGP_TYPE_PGP5;
	else if (!strcmp (bin, "gpg"))
		type = PGP_TYPE_GPG;
	
	mail_config_set_pgp_path (path && *path ? path : NULL);
	mail_config_set_pgp_type (type);
}

static void
set_color (GnomeColorPicker *cp)
{
	guint32 rgb = mail_config_get_citation_color ();

	gnome_color_picker_set_i8 (cp, (rgb & 0xff0000) >> 16, (rgb & 0xff00) >> 8, rgb & 0xff, 0xff);
}

static void
construct (MailAccountsDialog *dialog)
{
	GladeXML *gui;
	GtkWidget *notebook;
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "mail-accounts-dialog");
	dialog->gui = gui;
	
	/* get our toplevel widget */
	notebook = glade_xml_get_widget (gui, "notebook");
	
	/* reparent */
	gtk_widget_reparent (notebook, GNOME_DIALOG (dialog)->vbox);
	
	/* give our dialog an OK button and title */
	gtk_window_set_title (GTK_WINDOW (dialog), _("Evolution Account Manager"));
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, TRUE);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 300);
	gnome_dialog_append_button (GNOME_DIALOG (dialog), GNOME_STOCK_BUTTON_OK);
	
	dialog->mail_accounts = GTK_CLIST (glade_xml_get_widget (gui, "clistAccounts"));
	gtk_signal_connect (GTK_OBJECT (dialog->mail_accounts), "select-row",
			    GTK_SIGNAL_FUNC (mail_select), dialog);
	gtk_signal_connect (GTK_OBJECT (dialog->mail_accounts), "unselect-row",
			    GTK_SIGNAL_FUNC (mail_unselect), dialog);
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
	
#if defined (ENABLE_NNTP)
	dialog->news_accounts = GTK_CLIST (glade_xml_get_widget (gui, "clistAccounts"));
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
	
	/* get those temp widgets */
	dialog->send_html = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkSendHTML"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->send_html),
				      mail_config_get_send_html ());
	gtk_signal_connect (GTK_OBJECT (dialog->send_html), "toggled",
			    GTK_SIGNAL_FUNC (send_html_toggled), dialog);
	
	dialog->citation_highlight = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chckHighlightCitations"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->citation_highlight),
				      mail_config_get_citation_highlight ());
	gtk_signal_connect (GTK_OBJECT (dialog->citation_highlight), "toggled",
			    GTK_SIGNAL_FUNC (citation_highlight_toggled), dialog);

	dialog->citation_color = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, "colorpickerCitations"));
	set_color (dialog->citation_color);
	gtk_signal_connect (GTK_OBJECT (dialog->citation_color), "color_set",
			    GTK_SIGNAL_FUNC (citation_color_set), dialog);

	dialog->timeout = GTK_SPIN_BUTTON (glade_xml_get_widget (gui, "spinMarkTimeout"));
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->timeout),
				   (1.0 * mail_config_get_mark_as_seen_timeout ()) / 1000.0);
	gtk_signal_connect (GTK_OBJECT (dialog->timeout), "changed",
			    GTK_SIGNAL_FUNC (timeout_changed), dialog);
	
	dialog->pgp_path = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "filePgpPath"));
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (dialog->pgp_path)),
			    mail_config_get_pgp_path ());
	gnome_file_entry_set_default_path (dialog->pgp_path, mail_config_get_pgp_path ());
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (dialog->pgp_path)),
			    "changed", GTK_SIGNAL_FUNC (pgp_path_changed), dialog);
	
	/* now to fill in the clists */
	dialog->accounts_row = -1;
	dialog->accounts = mail_config_get_accounts ();
	if (dialog->accounts) {
		load_accounts (dialog);
		gtk_clist_select_row (dialog->mail_accounts, 0, 0);
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
