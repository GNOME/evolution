/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Parthasarathi Susarla <sparthasarathi@novell.com>
 *
 * Copyright 2004 Novell, Inc. (www.novell.com)
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
 * Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "mail-send-options.h"

#include "mail/em-menu.h"
#include "mail/em-utils.h"

#include "composer/e-msg-composer.h"
#include "e-util/e-account.h"

#include "misc/e-send-options.h"

static ESendOptionsDialog * dialog = NULL ;

void org_gnome_compose_send_options (EPlugin *ep, EMMenuTargetWidget *t);

static time_t 
add_day_to_time (time_t time, int days)
{
	struct tm *tm;

	tm = localtime (&time);
	tm->tm_mday += days;
	tm->tm_isdst = -1;

	return mktime (tm);
}

static void
feed_input_data(ESendOptionsDialog * dialog, gint state, gpointer data)
{
	EMsgComposer *comp;
	char value [100];
	char *temp = NULL;

	comp = (EMsgComposer *) data;
	/* we are bothered only for ok response: other cases are handled generally*/
	if (state == GTK_RESPONSE_OK) {	
		if (dialog->data->gopts->reply_enabled) {
			if (dialog->data->gopts->reply_convenient)
				e_msg_composer_add_header (comp, X_REPLY_CONVENIENT ,"1" ) ;
			else if (dialog->data->gopts->reply_within) {
				time_t t;
				t = add_day_to_time (time (NULL), dialog->data->gopts->reply_within);
				strftime (value, 17, "%Y%m%dT%H%M%SZ", gmtime (&t));
				e_msg_composer_add_header (comp, X_REPLY_WITHIN , value) ;
			}
		}

		if (dialog->data->gopts->expiration_enabled) {
			if (dialog->data->gopts->expire_after != 0) {
				time_t t;
				t = add_day_to_time (time (NULL), dialog->data->gopts->expire_after);
				strftime (value, 17, "%Y%m%dT%H%M%SZ", gmtime (&t));
				e_msg_composer_add_header (comp, X_EXPIRE_AFTER, value) ;
			}
		}
		if (dialog->data->gopts->delay_enabled) {
			strftime (value, 17, "%Y%m%dT%H%M%SZ", gmtime (&dialog->data->gopts->delay_until));
			e_msg_composer_add_header (comp, X_DELAY_UNTIL, value) ;
		}

		/*Status Tracking Options*/
		if (dialog->data->sopts->tracking_enabled) {
			temp = g_strdup_printf ("%d",dialog->data->sopts->track_when) ;
			e_msg_composer_add_header (comp, X_TRACK_WHEN, temp) ;
			g_free (temp) ;
		}

		if (dialog->data->sopts->autodelete) {
			e_msg_composer_add_header (comp, X_AUTODELETE, "1") ;
		}
		if (dialog->data->sopts->opened) {
			temp = g_strdup_printf ("%d",dialog->data->sopts->opened) ;
			e_msg_composer_add_header (comp, X_RETURN_NOTIFY_OPEN, temp) ;
			g_free (temp) ;
		}
		if (dialog->data->sopts->declined) {
			temp = g_strdup_printf ("%d",dialog->data->sopts->declined) ;
			e_msg_composer_add_header (comp, X_RETURN_NOTIFY_DELETE, temp) ;
			g_free (temp) ;
		}

		if (dialog->data->gopts->priority) {
			temp = g_strdup_printf ("%d",dialog->data->gopts->priority);
			e_msg_composer_add_header (comp, X_SEND_OPT_PRIORITY,temp);
			g_free (temp);
		}
	} 
}

static void
send_options_commit (EMsgComposer *comp, gpointer user_data)
{
	if (!user_data && !E_IS_SENDOPTIONS_DIALOG (user_data))
		return;
		
	if (dialog) {
		g_object_unref (dialog);
		dialog = NULL;	
	}
}

void 
org_gnome_compose_send_options (EPlugin *ep, EMMenuTargetWidget *t)
{
	struct _EMenuTarget menu = t->target ;

	EMsgComposer *comp = (struct _EMsgComposer *)menu.widget ;
	EAccount *account = NULL; 
	char *temp = NULL;
	char *url;
	
	account = e_msg_composer_get_preferred_account (comp) ;
	url = g_strdup (account->transport->url) ;
	temp = strstr (url, "groupwise") ;
	if (!temp) {
		g_print ("Sorry send options only available for a groupwise account\n") ;
		return;
	} 
	g_free (temp) ;
	/*disply the send options dialog*/
	if (!dialog) {
		g_print ("New dialog\n\n") ;
		dialog = e_sendoptions_dialog_new () ;
	}

	e_sendoptions_dialog_run (dialog, menu.widget, E_ITEM_MAIL) ;
	
	g_signal_connect (dialog, "sod_response",
				  G_CALLBACK (feed_input_data), comp);

	g_signal_connect (GTK_WIDGET (comp), "destroy",
				  G_CALLBACK (send_options_commit), dialog);
}


