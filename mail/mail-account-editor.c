/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors:
 *    Jeffrey Stedfast <fejj@ximian.com>
 *    Dan Winship <danw@ximian.com>
 *
 *  Copyright 2001-2003 Ximian, Inc. (www.ximian.com)
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
#include <e-util/e-account.h>

#include <gtk/gtknotebook.h>
#include <gtk/gtkstock.h>

#include "widgets/misc/e-error.h"

#include "em-account-prefs.h"
#include "mail-config.h"
#include "mail-account-editor.h"
#include "mail-account-gui.h"
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
		e_error_run(editor, "mail:account-incomplete", NULL);
		return FALSE;
	}
	
	if (mail_account_gui_save (editor->gui) == FALSE)
		return FALSE;
	
	/* save any changes we may have */
	mail_config_write ();
	
	return TRUE;
}

static void
editor_response_cb (GtkWidget *widget, int button, gpointer user_data)
{
	MailAccountEditor *editor = user_data;
	
	switch (button) {
	case GTK_RESPONSE_OK:
		apply_changes (editor);
	default:
		gtk_widget_destroy (GTK_WIDGET (editor));
	}
}

static void
construct (MailAccountEditor *editor, EAccount *account, EMAccountPrefs *dialog)
{
	EAccountService *source = account->source;
	
	gtk_widget_realize (GTK_WIDGET (editor));
	gtk_dialog_set_has_separator (GTK_DIALOG (editor), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (editor)->action_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (editor)->vbox), 0);

	editor->gui = mail_account_gui_new (account, dialog);
	
	/* get our toplevel widget and reparent it */
	editor->notebook = GTK_NOTEBOOK (glade_xml_get_widget (editor->gui->xml, "account_editor_notebook"));
	gtk_widget_reparent (GTK_WIDGET (editor->notebook), GTK_DIALOG (editor)->vbox);
	
	/* give our dialog an OK button and title */
	gtk_window_set_title (GTK_WINDOW (editor), _("Evolution Account Editor"));
	gtk_window_set_resizable (GTK_WINDOW (editor), TRUE);
	gtk_window_set_modal (GTK_WINDOW (editor), FALSE);
	gtk_dialog_add_buttons (GTK_DIALOG (editor),
				GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
	
	g_signal_connect (editor, "response", G_CALLBACK (editor_response_cb), editor);
	
	mail_account_gui_setup (editor->gui, GTK_WIDGET (editor));
	
	mail_account_gui_build_extra_conf (editor->gui, source->url);
	
	gtk_widget_grab_focus (GTK_WIDGET (editor->gui->account_name));
}

MailAccountEditor *
mail_account_editor_new (EAccount *account, GtkWindow *parent, EMAccountPrefs *dialog)
{
	MailAccountEditor *new;
	
	new = (MailAccountEditor *) g_object_new (mail_account_editor_get_type (), NULL);
	gtk_window_set_transient_for ((GtkWindow *) new, parent);
	construct (new, account, dialog);
	
	return new;
}
