/*
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
 *		R.Raghavendran <raghavguru7@gmail.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <camel/camel-mime-message.h>
#include <camel/camel-address.h>
#include "mail/em-event.h"

#include "composer/e-msg-composer.h"
#include "libedataserver/e-account.h"

#include "exchange-send-options.h"

void org_gnome_exchange_send_options (EPlugin *ep, EMEventTargetComposer *t);

static ExchangeSendOptionsDialog *dialog=NULL;

static void
append_to_header (ExchangeSendOptionsDialog *dialog, gint state, gpointer data)
{
	EMsgComposer *composer;
	CamelAddress *sender_address;
	const gchar *sender_id, *recipient_id;
	struct _camel_header_address *addr;
	struct _camel_header_address *sender_addr;

	composer = (EMsgComposer *)data;
	if (state == GTK_RESPONSE_OK) {
		if (dialog->options->importance) {
			switch (dialog->options->importance) {
				case E_IMP_HIGH :
					e_msg_composer_modify_header (composer, "Importance", "high");
					break;
				case E_IMP_LOW :
					e_msg_composer_modify_header (composer, "Importance", "low");
					break;
				default :
					g_print ("\nNo importance set");
					break;
			}
		}
		else
			e_msg_composer_remove_header (composer, "Importance");

		if (dialog->options->sensitivity) {
			switch (dialog->options->sensitivity) {
				case E_SENSITIVITY_CONFIDENTIAL :
					e_msg_composer_modify_header (composer, "Sensitivity", "Company-Confidential");
					break;
				case E_SENSITIVITY_PERSONAL :
					e_msg_composer_modify_header (composer, "Sensitivity", "Personal");
					break;
				case E_SENSITIVITY_PRIVATE :
					e_msg_composer_modify_header (composer, "Sensitivity", "Private");
					break;
				default :
					g_print ("\nNo importance set");
					break;
			}
		}
		else
			e_msg_composer_remove_header (composer, "Sensitivity");

		sender_address = (CamelAddress *) e_msg_composer_get_from (composer);
		sender_id = (const gchar *) camel_address_encode (sender_address);

		addr = camel_header_address_decode (dialog->options->delegate_address, NULL);
		sender_addr = camel_header_address_decode (sender_id, NULL);

		if (dialog->options->send_as_del_enabled &&
			dialog->options->delegate_address &&
				g_ascii_strcasecmp(addr->v.addr, sender_addr->v.addr)) {

			e_msg_composer_modify_header (composer, "Sender" , sender_id);

			/* This block handles the case wherein the address to be added
			 * in the "From" field has no name associated with it.
			 * So for cases where there is no name we append the address
			 * (only email) within angular braces.
			 */
			if (!g_ascii_strcasecmp (addr->name, "")) {
				recipient_id = g_strdup_printf ("<%s>",
						dialog->options->delegate_address);
				e_msg_composer_add_header (composer, "From", recipient_id);
			}

			else
				e_msg_composer_add_header (composer, "From",
							dialog->options->delegate_address);
		}

		else {
			e_msg_composer_remove_header (composer, "Sender");
			e_msg_composer_add_header (composer, "From", sender_id);
		}

		if (dialog->options->delivery_enabled) {
			EComposerHeaderTable *table;
			EAccount *account;
			gchar *mdn_address;

			table = e_msg_composer_get_header_table (composer);
			account = e_composer_header_table_get_account (table);
			mdn_address = account->id->reply_to;
			if (!mdn_address || !*mdn_address)
				mdn_address = account->id->address;
			e_msg_composer_modify_header (composer, "Return-Receipt-To", mdn_address);
		}
		else
			e_msg_composer_remove_header (composer, "Return-Receipt-To");

		if (dialog->options->read_enabled) {
			EComposerHeaderTable *table;
			EAccount *account;
			gchar *mdn_address;

			table = e_msg_composer_get_header_table (composer);
			account = e_composer_header_table_get_account (table);
			mdn_address = account->id->reply_to;
			if (!mdn_address || !*mdn_address)
				mdn_address = account->id->address;

			e_msg_composer_modify_header (composer, "Disposition-Notification-To", mdn_address);
		}
		else
			e_msg_composer_remove_header (composer, "Disposition-Notification-To");
	}
}

static void
send_options_commit (EMsgComposer *comp, gpointer user_data)
{
	if (!user_data && !EXCHANGE_IS_SENDOPTIONS_DIALOG (user_data))
		return;

	if (dialog) {
		g_print ("\nDialog getting unreferenced ");
		g_object_unref (dialog);
		dialog = NULL;
	}
}

void
org_gnome_exchange_send_options (EPlugin *ep, EMEventTargetComposer *target)
{
	EMsgComposer *composer = target->composer;
	EComposerHeaderTable *table;
	EAccount *account = NULL;
	gchar *temp = NULL;

	table = e_msg_composer_get_header_table (composer);
	account = e_composer_header_table_get_account (table);
	if (!account)
		return;

	temp = strstr (account->transport->url, "exchange");
	if (!temp) {
		return;
	}
	e_msg_composer_set_send_options (composer, TRUE);
	/*disply the send options dialog*/
	if (!dialog) {
		g_print ("New dialog\n\n");
		dialog = exchange_sendoptions_dialog_new ();
	}
	exchange_sendoptions_dialog_run (dialog, GTK_WIDGET (composer));
	g_signal_connect (dialog, "sod_response", G_CALLBACK (append_to_header), GTK_WIDGET (composer));

	g_signal_connect (GTK_WIDGET (composer), "destroy",
				  G_CALLBACK (send_options_commit), dialog);

}
