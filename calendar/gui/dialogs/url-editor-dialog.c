/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Gary Ekker <gekker@novell.com>
 *
 * Copyright 2004 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * UrlEditorDialog - a GtkObject which handles a libglade-loaded dialog
 * to edit the calendar preference settings.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "cal-prefs-dialog.h"
#include "url-editor-dialog.h"

#include "e-util/e-passwords.h"
#include <gtk/gtk.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktogglebutton.h>
#include <libgnomeui/gnome-color-picker.h>
#include <glade/glade.h>
#include <e-util/e-dialog-widgets.h>
#include <e-util/e-icon-factory.h>
#include <widgets/misc/e-dateedit.h>
#include <stdlib.h>
#include <string.h>

static gboolean get_widgets (UrlDialogData *data);
static void init_widgets (UrlDialogData *data);

static void url_editor_dialog_fb_url_changed (GtkEntry *url_entry, void *data);
static void url_editor_dialog_fb_daily_toggled (GtkWidget *button, void *data);
static void url_editor_dialog_fb_url_activated (GtkEntry *url_entry, void *data);
static void url_editor_dialog_fb_ok_enable (GtkWidget *widget, void *data);

/**
 * url_editor_dialog_new:
 *
 * Creates a new #UrlEditorDialog.
 *
 * Return value: a new #UrlEditorDialog.
 **/
gboolean
url_editor_dialog_new (DialogData *dialog_data, EPublishUri *uri)
{
	int b;

	UrlDialogData *url_dlg_data = g_new0 (UrlDialogData, 1);
	url_dlg_data->xml = glade_xml_new (EVOLUTION_GLADEDIR "/url-editor-dialog.glade", NULL, NULL);
	if (!url_dlg_data->xml) {
		g_message ("url_editor_dialog_construct(): Could not load the Glade XML file!");
		return FALSE;
	}

	if (!get_widgets (url_dlg_data)) {
		g_message ("url_editor_dialog_construct(): Could not find all widgets in the XML file!");
		return FALSE;
	}
	
	url_dlg_data->url_dialog = (GtkWidget *) dialog_data;
	url_dlg_data->url_data = uri;
	
	init_widgets (url_dlg_data);
	
	if (uri->location && uri->username) {
		if (strlen(uri->location) != 0) {
			gtk_entry_set_text (url_dlg_data->url_entry, 
					    uri->location);
		}
		if (strlen(uri->username) != 0) {
			gtk_entry_set_text (url_dlg_data->username_entry, 
					    uri->username);
		}
	}
	
	uri->password = e_passwords_get_password ("Calendar", url_dlg_data->url_data->location);
	
	if (uri->password) {
		if (strlen(uri->password) != 0) {
			gtk_entry_set_text (url_dlg_data->password_entry, 
					    uri->password);

			e_dialog_toggle_set (url_dlg_data->remember_pw, TRUE);
		} else {
			e_dialog_toggle_set (url_dlg_data->remember_pw, FALSE);
		}
	}
	
	switch (uri->publish_freq) {
		case URI_PUBLISH_DAILY:
			e_dialog_radio_set (url_dlg_data->daily, 
						URI_PUBLISH_DAILY, 
						pub_frequency_type_map);
			break;
		case URI_PUBLISH_WEEKLY:
			e_dialog_radio_set (url_dlg_data->daily, 
						URI_PUBLISH_WEEKLY, 
						pub_frequency_type_map);
			break;
		case URI_PUBLISH_USER:
		default:
			e_dialog_radio_set (url_dlg_data->daily, 
						URI_PUBLISH_USER, 
						pub_frequency_type_map);
	}

	dialog_data->url_editor=TRUE; 
	dialog_data->url_editor_dlg = (GtkWidget *) url_dlg_data;
	gtk_widget_set_sensitive ((GtkWidget *) url_dlg_data->ok, FALSE);
	
	b = gtk_dialog_run ((GtkDialog *) url_dlg_data->url_editor);
	
	if (b == GTK_RESPONSE_OK) {
		if ((GtkEntry *) url_dlg_data->url_entry) {
			url_editor_dialog_fb_url_activated (url_dlg_data->url_entry, url_dlg_data);
			url_dlg_data->url_data->username = g_strdup (gtk_entry_get_text ((GtkEntry *) url_dlg_data->username_entry));
			url_dlg_data->url_data->password = g_strdup (gtk_entry_get_text ((GtkEntry *) url_dlg_data->password_entry));
			if (e_dialog_toggle_get (url_dlg_data->remember_pw)) {
				e_passwords_add_password (url_dlg_data->url_data->location, url_dlg_data->url_data->password);
				e_passwords_remember_password ("Calendar", url_dlg_data->url_data->location);				
			} else {
				e_passwords_forget_password ("Calendar", url_dlg_data->url_data->location);
			}
		}
	}
	
	gtk_widget_destroy (url_dlg_data->url_editor);
	g_object_unref (url_dlg_data->xml);
	g_free (url_dlg_data);
	url_dlg_data = NULL;
	
	return FALSE;
}

static gboolean
get_widgets (UrlDialogData *data)
{
#define GW(name) glade_xml_get_widget (data->xml, name)

	data->url_editor = GW ("url_editor");
	data->calendar_list_label = GW ("calendar_list_label");
	data->url_dialog = GW ("fb_dialog");
	data->url_entry = GTK_ENTRY (GW ("url_entry"));
	data->daily = GW ("daily");
	data->weekly = GW ("weekly");
	data->user_publish = GW ("user_publish");
	data->scrolled_window =  GW ("scrolled_window");
	data->username_entry = GTK_ENTRY (GW ("username_entry"));
	data->password_entry = GTK_ENTRY (GW ("password_entry"));
	data->remember_pw = GW ("remember_pw");
	data->cancel = GW ("cancel");
	data->ok = GW ("ok");

#undef GW

	return (data ->url_editor
		&& data->calendar_list_label
		&& data->url_entry
		&& data->daily
		&& data->weekly
		&& data->user_publish
		&& data->scrolled_window
		&& data->username_entry
		&& data->password_entry
		&& data->remember_pw
		&& data->cancel
		&& data->ok);
}

static void
selection_changed_callback (ESourceSelector *selector,
			    void *data)
{
	UrlDialogData *url_dlg_data  = (UrlDialogData *) data;
	GSList *selection = e_source_selector_get_selection (selector);
	
	if (selection != NULL) {
		GSList *p, *l =  NULL;
		
		for (p = selection; p != NULL; p = p->next) {
			ESource *source = E_SOURCE(p->data);
			gchar* source_uid = g_strdup(e_source_peek_uid(source));
			
			l = g_slist_append (l, source_uid);
		}
		url_dlg_data->url_data->calendars = l;
	}
	
	e_source_selector_free_selection (selection);
	gtk_widget_set_sensitive ((GtkWidget *) url_dlg_data->ok, TRUE);
}

/* Connects any necessary signal handlers. */
static void
init_widgets (UrlDialogData *url_dlg_data)
{
	GtkWidget *selector;
	ESourceList *source_list;
	GConfClient *gconf_client;
	GList *icon_list;
	GSList *p;
	
	gtk_widget_ensure_style (url_dlg_data->url_editor);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (url_dlg_data->url_editor)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (url_dlg_data->url_editor)->action_area), 12);

	g_signal_connect (url_dlg_data->url_entry, "changed", 
			  G_CALLBACK (url_editor_dialog_fb_url_changed), 
			  url_dlg_data);
	
	g_signal_connect (url_dlg_data->username_entry, "changed", 
			  G_CALLBACK (url_editor_dialog_fb_ok_enable), 
			  url_dlg_data);
	
	g_signal_connect (url_dlg_data->password_entry, "changed", 
			  G_CALLBACK (url_editor_dialog_fb_ok_enable), 
			  url_dlg_data);
	
	g_signal_connect (url_dlg_data->remember_pw, "toggled",
			  G_CALLBACK (url_editor_dialog_fb_ok_enable),
			  url_dlg_data);
	
	g_signal_connect (url_dlg_data->url_entry, "activate",
			  G_CALLBACK (url_editor_dialog_fb_url_activated), 
			  url_dlg_data);
		  
	g_signal_connect (url_dlg_data->daily, "toggled",
			  G_CALLBACK (url_editor_dialog_fb_daily_toggled),
			  url_dlg_data);
	
	g_signal_connect (url_dlg_data->weekly, "toggled",
			  G_CALLBACK (url_editor_dialog_fb_daily_toggled),
			  url_dlg_data);

	g_signal_connect (url_dlg_data->user_publish, "toggled",
			  G_CALLBACK (url_editor_dialog_fb_daily_toggled),
			  url_dlg_data);


	if (url_dlg_data->url_data->calendars) {
		ESource *source;
		
		gconf_client = gconf_client_get_default ();
		source_list = e_source_list_new_for_gconf (gconf_client, "/apps/evolution/calendar/sources");
		selector = e_source_selector_new (source_list);
		
		p = url_dlg_data->url_data->calendars;
		for (; p != NULL; p = p->next) {
			gchar *source_uid;

			source_uid = g_strdup (p->data);
			source =  e_source_list_peek_source_by_uid (source_list, source_uid);			
			e_source_selector_select_source ((ESourceSelector *)selector, source);
			g_free (source_uid);
		}
	} else {
		gconf_client = gconf_client_get_default ();
		source_list = e_source_list_new_for_gconf (gconf_client, "/apps/evolution/calendar/sources");
		selector = e_source_selector_new (source_list);
	}
	e_source_selector_show_selection ((ESourceSelector *) selector, TRUE);
	g_signal_connect (selector, "selection_changed", 
			  G_CALLBACK (selection_changed_callback), 
			  url_dlg_data);

	gtk_label_set_mnemonic_widget (GTK_LABEL (url_dlg_data->calendar_list_label),
				       selector);
	gtk_widget_show (selector);
	gtk_container_add (GTK_CONTAINER (url_dlg_data->scrolled_window), 
			   selector);

	icon_list = e_icon_factory_get_icon_list ("stock_calendar");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (url_dlg_data->url_editor), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}
	
	gtk_widget_show (url_dlg_data->scrolled_window);
}

static void
url_editor_dialog_fb_daily_toggled (GtkWidget *button,
				      void *data)
{
	UrlDialogData *url_dlg_data  = (UrlDialogData *) data;
	enum publish_frequency frequency;
	
	frequency = e_dialog_radio_get (url_dlg_data->daily, 
					pub_frequency_type_map);
	url_dlg_data->url_data->publish_freq = frequency;
	gtk_widget_set_sensitive ((GtkWidget *) url_dlg_data->ok, TRUE);
}

static gboolean
is_valid_url (const gchar *url)
{
	const gchar *p = url;

	if (strlen (url) == 0) {
		return FALSE;
	}
	while (*p) {
		if ((*p == '\\') || (*p == ' ')) {
			return FALSE;
		}
		p++;
	}
	return TRUE;
}

static void
url_editor_dialog_fb_url_activated (GtkEntry *url_entry, void *data)
{
	UrlDialogData *url_dlg_data = (UrlDialogData *) data;
	
	url_dlg_data->url_data->location = g_strdup (gtk_entry_get_text ((GtkEntry *) url_entry));
}

static void
url_editor_dialog_fb_url_changed (GtkEntry *url_entry, void *data)
{
	UrlDialogData *url_dlg_data = (UrlDialogData *) data;
	DialogData *url_dialog = (DialogData *) url_dlg_data->url_dialog;
	
	const gchar *entry_contents;
	GtkListStore *model;
	GtkTreeIter iter;
	gboolean valid;

	model = (GtkListStore *) gtk_tree_view_get_model (url_dialog->url_list);
	
	entry_contents = gtk_entry_get_text ((GtkEntry *) url_entry);
	if (!is_valid_url (entry_contents)) {
		gtk_widget_set_sensitive ((GtkWidget *) url_dlg_data->ok, FALSE);
		return;
	}
	/* duplicate check */
	valid = gtk_tree_model_get_iter_first ((GtkTreeModel *) model, &iter);
	while (valid) {
		gchar *url_name;
		gtk_tree_model_get ((GtkTreeModel *) model, &iter, 
				    URL_LIST_LOCATION_COLUMN, &url_name, 
				    -1);

		if (!strcasecmp (url_name, entry_contents)) {
			gtk_widget_set_sensitive ((GtkWidget *) url_dlg_data->ok, FALSE);
			return;
		}
		valid = gtk_tree_model_iter_next ((GtkTreeModel *) model, &iter);
	}
	/* valid and unique */
	gtk_widget_set_sensitive (GTK_WIDGET (url_dlg_data->ok), TRUE);
	gtk_widget_grab_default (GTK_WIDGET (url_dlg_data->ok));
	gtk_entry_set_activates_default ((GtkEntry*) url_dlg_data->url_entry, 
					TRUE);
}

static void url_editor_dialog_fb_ok_enable (GtkWidget *widget, void *data) {
	UrlDialogData *url_dlg_data = (UrlDialogData *) data;
	
	gtk_widget_set_sensitive (GTK_WIDGET (url_dlg_data->ok), TRUE);
}
