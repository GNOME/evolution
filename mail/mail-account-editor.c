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
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include <camel/camel-url.h>
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-gui-utils.h>

#include "mail-account-editor.h"
#include "mail-session.h"

static void mail_account_editor_class_init (MailAccountEditorClass *class);
static void mail_account_editor_finalize   (GtkObject *obj);

static GnomeDialogClass *parent_class;


GtkType
mail_account_editor_get_type ()
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MailAccountEditor",
			sizeof (MailAccountEditor),
			sizeof (MailAccountEditorClass),
			(GtkClassInitFunc) mail_account_editor_class_init,
			(GtkObjectInitFunc) NULL,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gnome_dialog_get_type (), &type_info);
	}
	
	return type;
}

static void
mail_account_editor_class_init (MailAccountEditorClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) class;
	parent_class = gtk_type_class (gnome_dialog_get_type ());
	
	object_class->finalize = mail_account_editor_finalize;
}

static void
mail_account_editor_finalize (GtkObject *obj)
{
	MailAccountEditor *editor = (MailAccountEditor *) obj;
	
	mail_account_gui_destroy (editor->gui);
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

static gboolean
apply_changes (MailAccountEditor *editor)
{
	MailConfigAccount *account;
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
		gtk_notebook_set_page (editor->notebook, page);
		gtk_widget_grab_focus (incomplete);
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR, _("You have not filled in all of the required information."));
		return FALSE;
	}
	
	if (mail_account_gui_save (editor->gui) == FALSE)
		return FALSE;
	
	/* save any changes we may have */
	mail_config_write ();
	
	/* FIXME: #1549: if the account was a remote store, delete it from the folder-tree and re-add it */
	/* FIXME: preferably, we'd only do this if there were changes... oh well */
	
	return TRUE;
}

static void
apply_clicked (GtkWidget *widget, gpointer data)
{
	MailAccountEditor *editor = data;
	
	apply_changes (editor);
}

static void
ok_clicked (GtkWidget *widget, gpointer data)
{
	MailAccountEditor *editor = data;
	
	if (apply_changes (editor))
		gtk_widget_destroy (GTK_WIDGET (editor));
}

static void
cancel_clicked (GtkWidget *widget, gpointer data)
{
	MailAccountEditor *editor = data;
	
	gtk_widget_destroy (GTK_WIDGET (editor));
}

static void
construct (MailAccountEditor *editor, MailConfigAccount *account, MailAccountsTab *dialog)
{
	MailConfigService *source = account->source;
	
	editor->gui = mail_account_gui_new (account, dialog);
	
	/* get our toplevel widget and reparent it */
	editor->notebook = GTK_NOTEBOOK (glade_xml_get_widget (editor->gui->xml, "account_editor_notebook"));
	gtk_widget_reparent (GTK_WIDGET (editor->notebook), GNOME_DIALOG (editor)->vbox);
	
	/* give our dialog an OK button and title */
	gtk_window_set_title (GTK_WINDOW (editor), _("Evolution Account Editor"));
	gtk_window_set_policy (GTK_WINDOW (editor), FALSE, TRUE, TRUE);
	gtk_window_set_modal (GTK_WINDOW (editor), FALSE);
	gnome_dialog_append_buttons (GNOME_DIALOG (editor),
				     GNOME_STOCK_BUTTON_OK,
				     GNOME_STOCK_BUTTON_APPLY,
				     GNOME_STOCK_BUTTON_CANCEL,
				     NULL);
	
	gnome_dialog_button_connect (GNOME_DIALOG (editor), 0 /* OK */,
				     GTK_SIGNAL_FUNC (ok_clicked),
				     editor);
	gnome_dialog_button_connect (GNOME_DIALOG (editor), 1 /* APPLY */,
				     GTK_SIGNAL_FUNC (apply_clicked),
				     editor);
	gnome_dialog_button_connect (GNOME_DIALOG (editor), 2 /* CANCEL */,
				     GTK_SIGNAL_FUNC (cancel_clicked),
				     editor);
	
	mail_account_gui_setup (editor->gui, GTK_WIDGET (editor));
	
	mail_account_gui_build_extra_conf (editor->gui, source->url);
}

MailAccountEditor *
mail_account_editor_new (MailConfigAccount *account, GtkWindow *parent, MailAccountsTab *dialog)
{
	MailAccountEditor *new;
	
	new = (MailAccountEditor *) gtk_type_new (mail_account_editor_get_type ());
	gnome_dialog_set_parent (GNOME_DIALOG (new), parent);
	construct (new, account, dialog);
	
	return new;
}
