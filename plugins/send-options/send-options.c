/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Chenthill Palanisamy (pchenthill@novell.com)
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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

#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "e-util/e-account.h"
#include <widgets/misc/e-send-options.h>
#include <mail/em-config.h>
#include <e-gw-connection.h>
#include <camel/camel-url.h>
#include "e-util/e-passwords.h"

ESendOptionsDialog *sod = NULL;
GtkWidget *parent;
EGwConnection *n_cnc;
EGwSendOptions *opts = NULL;

void org_gnome_send_options (EPlugin *epl, EConfigHookItemFactoryData *data);
void send_options_commit (EPlugin *epl, EConfigHookItemFactoryData *data);
void send_options_changed (EPlugin *epl, EConfigHookItemFactoryData *data);
void send_options_abort (EPlugin *epl, EConfigHookItemFactoryData *data);

static EGwConnection * 
get_cnc (EAccount *account)
{
		EGwConnection *cnc;
		char *uri, *use_ssl, *property_value, *server_name, *user, *port, *pass = NULL;
		CamelURL *url;
		const char *poa_address;
		gboolean remember;
	
		url = camel_url_new (account->source->url, NULL);
		if (url == NULL) {
			return NULL;
		}
		poa_address = camel_url_get_param (url, "poa");
		if (!poa_address || strlen (poa_address) ==0)
			return NULL;

		server_name = g_strdup (url->host);
		user = g_strdup (url->user);
		property_value =  camel_url_get_param (url, "soap_port");
		use_ssl = g_strdup (camel_url_get_param (url, "soap_ssl"));
		if(property_value == NULL)
			port = g_strdup ("7181");
		else if (strlen(property_value) == 0)
			port = g_strdup ("7181");
		else
			port = g_strdup (property_value);

		if (use_ssl)
			uri = g_strconcat ("https://", server_name, ":", port, "/soap", NULL);	
		else
			uri = g_strconcat ("http://", server_name, ":", port, "/soap", NULL);

	 	pass = e_passwords_get_password ("Groupwise", uri);
		if (!pass)  {
			char *prompt;
			prompt = g_strdup_printf (_("%sEnter password for %s (user %s)"),
                                        "", poa_address, url->user);
					
		
			pass = e_passwords_ask_password (prompt, "Groupwise", uri, prompt,
                                                     E_PASSWORDS_REMEMBER_FOREVER|E_PASSWORDS_SECRET, &remember,
						     NULL);
			g_free (prompt);
		}
			
		cnc = e_gw_connection_new (uri, user, pass);
		camel_url_free (url);

		return cnc;

}


static void 
e_send_options_load_general_opts (ESendOptionsGeneral *gopts, EGwSendOptionsGeneral *ggopts)
{
	gopts->priority = ggopts->priority;

	gopts->reply_enabled = ggopts->reply_enabled;
	gopts->reply_convenient = ggopts->reply_convenient;
	
	gopts->expiration_enabled = ggopts->expiration_enabled;
	gopts->expire_after = ggopts->expire_after;

	if (!gopts->expire_after)
		gopts->expiration_enabled = FALSE;

	gopts->delay_enabled = ggopts->delay_enabled;
	
	/* TODO convert int to timet comparing the current day */
	if (ggopts->delay_until) {
	
	} else
		gopts->delay_until = 0;
}

static void
e_send_options_load_status_options (ESendOptionsStatusTracking *sopts, EGwSendOptionsStatusTracking *gsopts)
{
	sopts->tracking_enabled = gsopts->tracking_enabled;
	sopts->track_when = gsopts->track_when;

	sopts->autodelete = gsopts->autodelete;

	sopts->opened = gsopts->opened;
	sopts->accepted = gsopts->accepted;
	sopts->declined = gsopts->declined;
	sopts->completed = gsopts->completed;
}

static void
e_send_options_load_default_data (EGwSendOptions *opts, ESendOptionsDialog *sod) 
{
	EGwSendOptionsGeneral *ggopts;
	EGwSendOptionsStatusTracking *gmopts;	
	EGwSendOptionsStatusTracking *gcopts;	
	EGwSendOptionsStatusTracking *gtopts;	

	ggopts = e_gw_sendoptions_get_general_options (opts);
	gmopts = e_gw_sendoptions_get_status_tracking_options (opts, "mail");
	gcopts = e_gw_sendoptions_get_status_tracking_options (opts, "calendar");
	gtopts = e_gw_sendoptions_get_status_tracking_options (opts, "task");

	e_send_options_load_general_opts (sod->data->gopts, ggopts);
	e_send_options_load_status_options (sod->data->mopts, gmopts);
	e_send_options_load_status_options (sod->data->copts, gcopts);
	e_send_options_load_status_options (sod->data->topts, gtopts);
}

static void
e_sendoptions_clicked_cb (GtkWidget *button, gpointer data)
{
	EAccount *account;
       	
	account = (EAccount *) data;
	if (!sod) {
		sod = e_sendoptions_dialog_new ();	
		e_sendoptions_set_global (sod, TRUE);
		if (!n_cnc) 
			n_cnc = get_cnc (account);

		if (!n_cnc) {
			g_warning ("Send Options: Could not get the connection to the server \n");
			return;
		}
			
		e_gw_connection_get_settings (n_cnc, &opts);
		e_send_options_load_default_data (opts, sod);
	}
	
	if (n_cnc)
		e_sendoptions_dialog_run (sod, parent ? parent : NULL, E_ITEM_NONE);
	else
	       	return;
}

void
org_gnome_send_options (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	GtkWidget *frame, *button, *label;
	EAccount *account;
	
	target_account = (EMConfigTargetAccount *)data->config->target;
	account = target_account->account;

	if(!g_strrstr (account->source->url, "groupwise://"))
		return;
	
	frame = gtk_frame_new ("");
	label = gtk_frame_get_label_widget (GTK_FRAME (frame));
	gtk_label_set_markup (GTK_LABEL (label), "<b>Send Options</b>"); 
	button = gtk_button_new_with_label ("Advanced send options");
	gtk_widget_show (button);

	g_signal_connect(button, "clicked", 
			    G_CALLBACK (e_sendoptions_clicked_cb), account);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (data->parent));
	if (!GTK_WIDGET_TOPLEVEL (parent))
		parent = NULL;

	gtk_widget_set_size_request (button, -1, -1);
	gtk_container_add (GTK_CONTAINER (frame), button);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 12);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_widget_show (frame);	
	gtk_box_set_spacing (GTK_BOX (data->parent), 12);
	gtk_box_pack_start (GTK_BOX (data->parent), frame, FALSE, FALSE, 0);
}

static void
send_options_finalize ()
{
	if (n_cnc) {
		g_object_unref (n_cnc);
		n_cnc = NULL;
	}
	
	if (sod) {
		g_object_unref (sod);
		sod = NULL;
	}

	if (opts) {
		g_object_unref (opts);
		opts = NULL;
	}
}

void
send_options_commit (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	send_options_finalize ();
}

void
send_options_changed (EPlugin *epl, EConfigHookItemFactoryData *data)
{
}

void
send_options_abort (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	send_options_finalize ();
}


