/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors:
 *    Jeffrey Stedfast <fejj@ximian.com>
 *    Dan Winship <danw@ximian.com>
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
#include <camel/camel-url.h>
#include <e-util/e-dialog-utils.h>

#include "mail-account-editor.h"
#include "mail-session.h"

static void mail_account_editor_class_init (MailAccountEditorClass *class);
static void mail_account_editor_finalize   (GObject *obj);

static GtkDialogClass *parent_class = NULL;

GType
mail_account_editor_get_type ()
{
	static GtkType type = 0;
	
	if (!type) {
		GTypeInfo type_info = {
			sizeof (MailAccountEditorClass),
			NULL, NULL,
			(GClassInitFunc) mail_account_editor_class_init,
			NULL, NULL,
			sizeof (MailAccountEditor),
			0,
			NULL
		};
		
		type = g_type_register_static (gtk_dialog_get_type (), "MailAccountEditor", &type_info, 0);
	}
	
	return type;
}

static void
mail_account_editor_class_init (MailAccountEditorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref(gtk_dialog_get_type ());
	
	gobject_class->finalize = mail_account_editor_finalize;
}

static void
mail_account_editor_finalize (GObject *obj)
{
	MailAccountEditor *editor = (MailAccountEditor *) obj;
	
	mail_account_gui_destroy (editor->gui);
	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static gboolean
apply_changes (MailAccountEditor *editor)
{
	GtkWidget *incomplete;
	int page = -1;
	
	if (!mail_account_gui_identity_complete (editor->gui, &incomplete) ||
	    !mail_account_gui_management_complete (editor->gui, &incomplete))
		page = 0;
	else if (!mail_account_gui_source_complete (editor->gui, &incomplete))
		page = 1;
	else if (!mail_account_gui_transport_complete (editor->gui, &incomplete))
		page = 3;
	
	if (page != -1) {
		gtk_notebook_set_current_page (editor->notebook, page);
		gtk_widget_grab_focus (incomplete);
		e_notice (editor, GTK_MESSAGE_ERROR, _("You have not filled in all of the required information."));
		return FALSE;
	}
	
	if (mail_account_gui_save (editor->gui) == FALSE)
		return FALSE;

	gtk_dialog_set_response_sensitive (GTK_DIALOG (editor),
					   GTK_RESPONSE_APPLY, FALSE);
 
	/* save any changes we may have */
	mail_config_write ();
	
	/* FIXME: #1549: if the account was a remote store, delete it from the folder-tree and re-add it */
	/* FIXME: preferably, we'd only do this if there were changes... oh well */
	
	return TRUE;
}

static void
editor_response_cb (GtkWidget *widget, int button, gpointer user_data)
{
	MailAccountEditor *editor = user_data;
	
	switch (button) {
	case GTK_RESPONSE_APPLY:
		apply_changes (editor);
		return;
	case GTK_RESPONSE_OK:
		apply_changes (editor);
	default:
		gtk_widget_destroy (GTK_WIDGET (editor));
	}
}

static void
mail_account_editor_changed (GtkWidget *widget, MailAccountEditor *editor)
{
	gtk_dialog_set_response_sensitive (GTK_WIDGET (editor), GTK_RESPONSE_APPLY, TRUE);
}

static void
construct (MailAccountEditor *editor, EAccount *account, MailAccountsTab *dialog)
{
	EAccountService *source = account->source;
	
	editor->gui = mail_account_gui_new (account, dialog);
	
	/* get our toplevel widget and reparent it */
	editor->notebook = GTK_NOTEBOOK (glade_xml_get_widget (editor->gui->xml, "account_editor_notebook"));
	gtk_widget_reparent (GTK_WIDGET (editor->notebook), GTK_DIALOG (editor)->vbox);
	
	/* give our dialog an OK button and title */
	gtk_window_set_title (GTK_WINDOW (editor), _("Evolution Account Editor"));
	gtk_window_set_resizable (GTK_WINDOW (editor), TRUE);
	gtk_window_set_modal (GTK_WINDOW (editor), FALSE);
	gtk_dialog_add_buttons (GTK_DIALOG (editor),
				GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
				GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);

	g_signal_connect (editor, "response", G_CALLBACK (editor_response_cb), editor);

	g_signal_connect (editor->gui->account_name, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->default_account, "toggled", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->full_name, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->email_address, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->reply_to, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->organization, "changed", G_CALLBACK (mail_account_editor_changed), editor);

	g_signal_connect (editor->gui->source.type, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->source.hostname, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->source.username, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->source.path, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->source.use_ssl, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->source.authtype, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->source.remember, "toggled", G_CALLBACK (mail_account_editor_changed), editor);

	g_signal_connect (editor->gui->source_auto_check, "toggled", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->source_auto_check_min, "value-changed", G_CALLBACK (mail_account_editor_changed), editor);

	g_signal_connect (editor->gui->transport.type, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->transport.hostname, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->transport.username, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->transport_needs_auth, "toggled", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->transport.use_ssl, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->transport.authtype, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->transport.remember, "toggled", G_CALLBACK (mail_account_editor_changed), editor);

	g_signal_connect (editor->gui->drafts_folder_button, "clicked", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->sent_folder_button, "clicked", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->always_cc, "toggled", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->cc_addrs, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->always_bcc, "toggled", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->bcc_addrs, "changed", G_CALLBACK (mail_account_editor_changed), editor);

	g_signal_connect (editor->gui->pgp_key, "changed", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->pgp_encrypt_to_self, "toggled", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->pgp_always_sign, "toggled", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->pgp_no_imip_sign, "toggled", G_CALLBACK (mail_account_editor_changed), editor);
	g_signal_connect (editor->gui->pgp_always_trust, "toggled", G_CALLBACK (mail_account_editor_changed), editor);

	mail_account_gui_setup (editor->gui, GTK_WIDGET (editor));

	mail_account_gui_build_extra_conf (editor->gui, source->url);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (editor),
					   GTK_RESPONSE_APPLY, FALSE);

	gtk_widget_grab_focus (GTK_WIDGET (editor->gui->account_name));
}

MailAccountEditor *
mail_account_editor_new (EAccount *account, GtkWindow *parent, MailAccountsTab *dialog)
{
	MailAccountEditor *new;
	
	new = (MailAccountEditor *) g_object_new (mail_account_editor_get_type (), NULL);
	gtk_window_set_transient_for ((GtkWindow *) new, parent);
	construct (new, account, dialog);
	
	return new;
}
