/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Cloned from mail-account-editor by Sam Creasey <sammy@oh.verio.com> 
 *
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

#include "mail-account-editor-news.h"
#include "mail-session.h"

static void mail_account_editor_news_class_init (MailAccountEditorNewsClass *class);
static void mail_account_editor_news_finalize   (GtkObject *obj);

static GnomeDialogClass *parent_class;


GtkType
mail_account_editor_news_get_type ()
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo type_info = {
			"MailAccountEditorNews",
			sizeof (MailAccountEditorNews),
			sizeof (MailAccountEditorNewsClass),
			(GtkClassInitFunc) mail_account_editor_news_class_init,
			(GtkObjectInitFunc) NULL,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		type = gtk_type_unique (gnome_dialog_get_type (), &type_info);
	}

	return type;
}

static void
mail_account_editor_news_class_init (MailAccountEditorNewsClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;
	parent_class = gtk_type_class (gnome_dialog_get_type ());

	object_class->finalize = mail_account_editor_news_finalize;
}

static void
mail_account_editor_news_finalize (GtkObject *obj)
{
	MailAccountEditorNews *editor = (MailAccountEditorNews *) obj;

	gtk_object_unref (GTK_OBJECT (editor->xml));

        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

static gboolean
apply_changes(MailAccountEditorNews *editor)
{
	
	CamelURL *url;
	GtkEntry *service_ent;
	
	service_ent = GTK_ENTRY(glade_xml_get_widget(editor->xml, "source_name"));
	url = g_new0 (CamelURL, 1);

	url->protocol = g_strdup("nntp");
	url->host = g_strdup(gtk_entry_get_text(service_ent));
	if(strlen(url->host) == 0) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR, _("You have not filled in all of the required information."));
		camel_url_free(url);
		return FALSE;
	}
	
	if(editor->service->url == NULL) 
		mail_config_add_news(editor->service);
	
	editor->service->url = camel_url_to_string(url, 0);
	
	mail_config_write();
	return TRUE;
}

static void
apply_clicked (GtkWidget *widget, gpointer data)
{
	MailAccountEditorNews *editor = data;

	apply_changes (editor);
}

static void
ok_clicked (GtkWidget *widget, gpointer data)
{
	MailAccountEditorNews *editor = data;

	if (apply_changes (editor))
		gtk_widget_destroy (GTK_WIDGET (editor));
}

static void
cancel_clicked (GtkWidget *widget, gpointer data)
{
	MailAccountEditorNews *editor = data;

	gtk_widget_destroy (GTK_WIDGET (editor));
}

GtkWidget *
mail_account_editor_news_new (MailConfigService *service)
{
	MailAccountEditorNews *editor;
	GtkEntry *service_ent;

	editor = (MailAccountEditorNews *) gtk_type_new (mail_account_editor_news_get_type ());
	
	editor->service = service;
	editor->xml = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", NULL);
	
	/* get our toplevel widget and reparent it */
	editor->notebook = GTK_NOTEBOOK (glade_xml_get_widget (editor->xml, "news_editor_notebook"));
	gtk_widget_reparent (GTK_WIDGET (editor->notebook), GNOME_DIALOG (editor)->vbox);
	
	/* give our dialog an OK button and title */
	gtk_window_set_title (GTK_WINDOW (editor), _("Evolution News Editor"));
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
	
	if (service->url) {
		CamelURL *url;
		
		url = camel_url_new (service->url, NULL);
		
		if (url->host) {
			service_ent = GTK_ENTRY (glade_xml_get_widget (editor->xml, "source_name"));
			gtk_entry_set_text (service_ent, url->host);
		}
		
		camel_url_free (url);
	}
	
	return GTK_WIDGET (editor);
}
