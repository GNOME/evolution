/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Parthasarathi Susarla <sparthasarathi@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
#include "mail/em-event.h"

#include "composer/e-msg-composer.h"
#include "libedataserver/e-account.h"

#include "misc/e-send-options.h"

static ESendOptionsDialog * dialog = NULL;

void org_gnome_composer_send_options (EPlugin *ep, EMEventTargetComposer *t);
void org_gnome_composer_message_reply (EPlugin *ep, EMEventTargetMessage *t);

static time_t
add_day_to_time (time_t time, gint days)
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
	gchar value [100];
	gchar *temp = NULL;

	comp = (EMsgComposer *) data;
	/* we are bothered only for ok response: other cases are handled generally*/
	if (state == GTK_RESPONSE_OK) {
		if (dialog->data->gopts->reply_enabled) {
			if (dialog->data->gopts->reply_convenient)
				e_msg_composer_add_header (comp, X_REPLY_CONVENIENT ,"1" );
			else if (dialog->data->gopts->reply_within) {
				time_t t;
				t = add_day_to_time (time (NULL), dialog->data->gopts->reply_within);
				strftime (value, 17, "%Y%m%dT%H%M%SZ", gmtime (&t));
				e_msg_composer_add_header (comp, X_REPLY_WITHIN , value);
			}
		}

		if (dialog->data->gopts->expiration_enabled) {
			if (dialog->data->gopts->expire_after != 0) {
				time_t t;
				t = add_day_to_time (time (NULL), dialog->data->gopts->expire_after);
				strftime (value, 17, "%Y%m%dT%H%M%SZ", gmtime (&t));
				e_msg_composer_add_header (comp, X_EXPIRE_AFTER, value);
			}
		}
		if (dialog->data->gopts->delay_enabled) {
			strftime (value, 17, "%Y%m%dT%H%M%SZ", gmtime (&dialog->data->gopts->delay_until));
			e_msg_composer_add_header (comp, X_DELAY_UNTIL, value);
		}

		/*Status Tracking Options*/
		if (dialog->data->sopts->tracking_enabled) {
			temp = g_strdup_printf ("%d",dialog->data->sopts->track_when);
			e_msg_composer_add_header (comp, X_TRACK_WHEN, temp);
			g_free (temp);
		}

		if (dialog->data->sopts->autodelete) {
			e_msg_composer_add_header (comp, X_AUTODELETE, "1");
		}
		if (dialog->data->sopts->opened) {
			temp = g_strdup_printf ("%d",dialog->data->sopts->opened);
			e_msg_composer_add_header (comp, X_RETURN_NOTIFY_OPEN, temp);
			g_free (temp);
		}
		if (dialog->data->sopts->declined) {
			temp = g_strdup_printf ("%d",dialog->data->sopts->declined);
			e_msg_composer_add_header (comp, X_RETURN_NOTIFY_DELETE, temp);
			g_free (temp);
		}

		if (dialog->data->gopts->priority) {
			temp = g_strdup_printf ("%d",dialog->data->gopts->priority);
			e_msg_composer_add_header (comp, X_SEND_OPT_PRIORITY, temp);
			g_free (temp);
		}

		if (dialog->data->gopts->security) {
			temp = g_strdup_printf ("%d",dialog->data->gopts->security);
			e_msg_composer_add_header (comp, X_SEND_OPT_SECURITY, temp);
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
org_gnome_composer_send_options (EPlugin *ep, EMEventTargetComposer *t)
{

	EMsgComposer *comp = (struct _EMsgComposer *)t->composer;
	EComposerHeaderTable *table;
	EAccount *account = NULL;
	gchar *temp = NULL;

	table = e_msg_composer_get_header_table (comp);
	account = e_composer_header_table_get_account (table);
	if (!account)
		return;

	temp = strstr (account->transport->url, "groupwise");
	if (!temp) {
		return;
	}
	e_msg_composer_set_send_options (comp, TRUE);
	/* display the send options dialog */
	if (!dialog) {
		g_print ("New dialog\n\n");
		dialog = e_sendoptions_dialog_new ();
	}
	e_sendoptions_dialog_run (dialog, GTK_WIDGET (comp), E_ITEM_MAIL);

	g_signal_connect (dialog, "sod_response",
				  G_CALLBACK (feed_input_data), comp);

	g_signal_connect (GTK_WIDGET (comp), "destroy",
				  G_CALLBACK (send_options_commit), dialog);
}

void
org_gnome_composer_message_reply (EPlugin *ep, EMEventTargetMessage *t)
{
	EMsgComposer *comp = (struct _EMsgComposer *)t->composer;
	EComposerHeaderTable *table;
	EAccount *account = NULL;
	gchar *temp = NULL;

	table = e_msg_composer_get_header_table (comp);
	account = e_composer_header_table_get_account (table);
	if (!account)
		return;

	temp = strstr (account->transport->url, "groupwise");
	if (!temp) {
		return;
	}
	e_msg_composer_add_header (comp, "X-GW-ORIG-ITEM-ID", t->uid);
}
