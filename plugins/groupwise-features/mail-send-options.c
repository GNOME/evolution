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

#include "mail/em-utils.h"
#include "mail/em-event.h"

#include "composer/e-msg-composer.h"
#include "composer/e-composer-from-header.h"
#include "libedataserver/e-account.h"

#include "misc/e-send-options.h"

static ESendOptionsDialog * dialog = NULL;

gboolean gw_ui_composer_actions (GtkUIManager *manager, EMsgComposer *composer);
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
	gchar value[100];
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

static gboolean
account_is_groupwise (EAccount *account)
{
	const gchar *url;

	if (!account)
		return FALSE;

	url = e_account_get_string (account, E_ACCOUNT_TRANSPORT_URL);
	return url && g_str_has_prefix (url, "groupwise://");
}

static void
from_changed_cb (EComposerFromHeader *header, EMsgComposer *composer)
{
	GtkActionGroup *group;
	GtkAction *action;
	EAccount *account;

	g_return_if_fail (header != NULL);
	g_return_if_fail (composer != NULL);

	group = gtkhtml_editor_get_action_group (GTKHTML_EDITOR (composer), "composer");
	g_return_if_fail (group != NULL);

	action = gtk_action_group_get_action (group, "gw-send-options");
	g_return_if_fail (action != NULL);

	account = e_composer_from_header_get_active (header);
	gtk_action_set_visible (action, account_is_groupwise (account));
}

static void
action_send_options_cb (GtkAction *action, EMsgComposer *composer)
{
	g_return_if_fail (action != NULL);
	g_return_if_fail (composer != NULL);

	/* display the send options dialog */
	if (!dialog) {
		dialog = e_send_options_dialog_new ();
	}

	e_send_options_dialog_run (dialog, GTK_WIDGET (composer), E_ITEM_MAIL);

	g_signal_connect (dialog, "sod_response",
			  G_CALLBACK (feed_input_data), composer);

	g_signal_connect (GTK_WIDGET (composer), "destroy",
			  G_CALLBACK (send_options_commit), dialog);
}

gboolean
gw_ui_composer_actions (GtkUIManager *manager, EMsgComposer *composer)
{
	static GtkActionEntry entries[] = {
		{ "gw-send-options",
		  NULL,
		  N_("_Send Options"),
		  NULL,
		  N_("Insert Send options"),
		  G_CALLBACK (action_send_options_cb) }
	};

	GtkhtmlEditor *editor;
	EComposerHeaderTable *headers;
	EComposerHeader *header;

	editor = GTKHTML_EDITOR (composer);

	/* Add actions to the "composer" action group. */
	gtk_action_group_add_actions (
		gtkhtml_editor_get_action_group (editor, "composer"),
		entries, G_N_ELEMENTS (entries), composer);

	headers = e_msg_composer_get_header_table (composer);
	header = e_composer_header_table_get_header (headers, E_COMPOSER_HEADER_FROM);

	from_changed_cb (E_COMPOSER_FROM_HEADER (header), composer);
	g_signal_connect (
		header, "changed",
		G_CALLBACK (from_changed_cb), composer);

	return TRUE;
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
