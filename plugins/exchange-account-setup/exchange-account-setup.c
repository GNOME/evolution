/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Sushma Rai <rsushma@novell.com>
 *  Copyright (C) 2004 Novell, Inc.
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <gtk/gtkdialog.h>
#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "e-util/e-account.h"

int e_plugin_lib_enable (EPluginLib *ep, int enable);

GtkWidget* org_gnome_exchange_account_setup (EPlugin *epl, EConfigHookItemFactoryData *data);

static void
oof_get()
{
	/* Read the oof state and oof message */
}

static void
oof_set ()
{
	/* store the oof state and message */
}

static GtkWidget *
create_page ()
{
	GtkWidget *oof_page;
	GtkWidget *oof_table;
	GtkWidget *oof_description, *label_status, *label_empty;
	GtkWidget *radiobutton_inoff, *radiobutton_oof;
	GtkWidget *vbox_oof, *vbox_oof_message;
	GtkWidget *oof_frame;
	GtkWidget *scrolledwindow_oof;
	GtkWidget *textview_oof;

	oof_page = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (oof_page), 12);

	/* Description section */

	oof_description = gtk_label_new (_("The message specified below will be automatically sent to \neach person who sends mail to you while you are out of the office."));
	gtk_label_set_justify (GTK_LABEL (oof_description), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (oof_description), TRUE);
	gtk_misc_set_alignment (GTK_MISC (oof_description), 0.5, 0.5);
	gtk_misc_set_padding (GTK_MISC (oof_description), 0, 18);
	
	gtk_box_pack_start (GTK_BOX (oof_page), oof_description, FALSE, TRUE, 0);

	/* Table with out of office radio buttons */

	oof_table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (oof_table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (oof_table), 6);
	gtk_box_pack_start (GTK_BOX (oof_page), oof_table, FALSE, FALSE, 0);

	label_status = gtk_label_new (_("<b>Status:</b>"));
	gtk_label_set_justify (GTK_LABEL (label_status), GTK_JUSTIFY_CENTER);
	gtk_misc_set_alignment (GTK_MISC (label_status), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label_status), 0, 0); 
	gtk_label_set_use_markup (GTK_LABEL (label_status), TRUE);
	gtk_table_attach (GTK_TABLE (oof_table), label_status, 0, 1, 0, 1, 
			  GTK_FILL, GTK_FILL, 0, 0); 

	radiobutton_inoff = gtk_radio_button_new_with_label (NULL, 
						_("I am in the office"));
	gtk_table_attach (GTK_TABLE (oof_table), radiobutton_inoff, 1, 2, 0, 1, 
			  GTK_FILL, GTK_FILL, 0, 0);

	label_empty = gtk_label_new (_(""));
	gtk_label_set_justify (GTK_LABEL (label_empty), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label_empty), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label_empty), 0, 0);
	gtk_label_set_use_markup (GTK_LABEL (label_empty), FALSE);
	gtk_table_attach (GTK_TABLE (oof_table), label_empty, 0, 1, 1, 2, 
			  GTK_FILL, GTK_FILL, 0, 0);

	radiobutton_oof = gtk_radio_button_new_with_label_from_widget (
					GTK_RADIO_BUTTON (radiobutton_inoff), 
					_("I am out of the office"));


	gtk_table_attach (GTK_TABLE (oof_table), radiobutton_oof, 1, 2, 1, 2, 
			  GTK_FILL, GTK_FILL, 0, 0);

	/* frame containg oof message text box */

	vbox_oof = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (oof_page), vbox_oof, FALSE, FALSE, 0);

	oof_frame = gtk_frame_new ("");
	gtk_container_set_border_width (GTK_CONTAINER (oof_frame), 1); 
	gtk_frame_set_shadow_type (GTK_FRAME (oof_frame), GTK_SHADOW_ETCHED_IN);
	gtk_frame_set_label (GTK_FRAME (oof_frame), _("Out of office Message:"));
	gtk_box_pack_start (GTK_BOX (vbox_oof), oof_frame, FALSE, FALSE, 0);

	vbox_oof_message = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (oof_frame), vbox_oof_message);
	
	scrolledwindow_oof = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow_oof), 
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
			GTK_SCROLLED_WINDOW (scrolledwindow_oof), 
			GTK_SHADOW_IN); 
	gtk_box_pack_start (GTK_BOX (vbox_oof_message), 
			    scrolledwindow_oof, TRUE, TRUE, 0);

	textview_oof = gtk_text_view_new(); 
	gtk_text_view_set_justification (GTK_TEXT_VIEW (textview_oof), 
					 GTK_JUSTIFY_LEFT);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview_oof), 
				     GTK_WRAP_WORD);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (textview_oof), TRUE);
	gtk_container_add (GTK_CONTAINER (scrolledwindow_oof), textview_oof);	
	gtk_widget_show_all (scrolledwindow_oof);

	gtk_widget_show_all (oof_page);

	return oof_page;
}

static GtkWidget *
construct_oof_editor (EConfigHookItemFactoryData *data)
{
	/* add oof page to editor */
	GtkWidget *oof_page;
	GladeXML *parent_xml;
	GtkNotebook *editor_notebook;
	GtkWidget *page_label;
	
	parent_xml = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", 
					   "account_editor_notebook", 
					   NULL);

	editor_notebook = (GtkNotebook *) glade_xml_get_widget (parent_xml, 
						"account_editor_notebook");
	if (!editor_notebook) {
		return NULL;
	}

	oof_page = create_page ();
	if (!oof_page) {
		return NULL;
	}
	
	page_label = gtk_label_new (_("Exchange Settings"));

	gtk_notebook_insert_page (GTK_NOTEBOOK (data->parent), 
				  oof_page, page_label, 4);
	return oof_page;
}

#if 0
int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{
	if (enable) {
	}
	return 0;
}
#endif


GtkWidget *
org_gnome_exchange_account_setup(EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	GtkWidget *oof_page;
	char *account_url = NULL, *exchange_url = NULL;

	target_account = (EMConfigTargetAccount *)data->config->target;
	account_url = g_strdup (target_account->account->source->url);
	exchange_url = g_strrstr (account_url, "exchange");
	g_free (account_url);

	if (exchange_url) { 
		printf ("org_gnome_exchange_account_setup\n"); 
		if (data->old) {
			printf ("return with old data \n"); 
			return data->old;
		}
		oof_page = construct_oof_editor (data);
		return oof_page;
	}
	else
		return NULL;
}
