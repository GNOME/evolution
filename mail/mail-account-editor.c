/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors:
 *    Jeffrey Stedfast <fejj@ximian.com>
 *    Dan Winship <danw@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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
	int page = -1;

	if (!mail_account_gui_identity_complete (editor->gui) ||
	    !mail_account_gui_management_complete (editor->gui))
		page = 0;
	else if (!mail_account_gui_source_complete (editor->gui))
		page = 1;
	else if (!mail_account_gui_transport_complete (editor->gui))
		page = 3;

	if (page != -1) {
		gtk_notebook_set_page (editor->notebook, page);
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR, _("You have not filled in all of the required information."));
		return FALSE;
	}

	mail_account_gui_save (editor->gui);
	account = editor->gui->account;

	/* save any changes we may have */
	mail_config_write ();
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
construct (MailAccountEditor *editor, MailConfigAccount *account)
{
	MailConfigService *source = account->source;
	
	editor->gui = mail_account_gui_new (account);
	
	/* get our toplevel widget and reparent it */
	editor->notebook = GTK_NOTEBOOK (glade_xml_get_widget (editor->gui->xml, "account_editor_notebook"));
	gtk_widget_reparent (GTK_WIDGET (editor->notebook), GNOME_DIALOG (editor)->vbox);
	
	/* give our dialog an OK button and title */
	gtk_window_set_title (GTK_WINDOW (editor), _("Evolution Account Editor"));
	gtk_window_set_policy (GTK_WINDOW (editor), FALSE, TRUE, TRUE);
	gtk_window_set_modal (GTK_WINDOW (editor), TRUE);
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
mail_account_editor_new (MailConfigAccount *account)
{
	MailAccountEditor *new;

	new = (MailAccountEditor *) gtk_type_new (mail_account_editor_get_type ());
	construct (new, account);

	return new;
}
