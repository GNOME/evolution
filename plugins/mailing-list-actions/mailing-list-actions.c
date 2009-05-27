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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2004 Meilof Veeningen <meilof@wanadoo.nl>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include "camel/camel-multipart.h"
#include "camel/camel-mime-part.h"
#include "camel/camel-exception.h"
#include "camel/camel-folder.h"
#include "composer/e-msg-composer.h"
#include "mail/em-composer-utils.h"
#include "mail/em-format-hook.h"
#include "mail/em-format.h"
#include "mail/em-menu.h"
#include "mail/em-config.h"
#include "mail/mail-ops.h"
#include "mail/mail-mt.h"
#include "mail/mail-config.h"
#include "e-util/e-util.h"
#include "e-util/e-error.h"

typedef enum {
	EMLA_ACTION_HELP,
	EMLA_ACTION_UNSUBSCRIBE,
	EMLA_ACTION_SUBSCRIBE,
	EMLA_ACTION_POST,
	EMLA_ACTION_OWNER,
	EMLA_ACTION_ARCHIVE
} EmlaAction;

typedef struct {
	EmlaAction action;    /* action enumeration */
	gboolean interactive; /* whether the user needs to edit a mailto: message (e.g. for post action) */
	const gchar * header;   /* header representing the action */
} EmlaActionHeader;

const EmlaActionHeader emla_action_headers[] = {
	{ EMLA_ACTION_HELP,        FALSE, "List-Help" },
	{ EMLA_ACTION_UNSUBSCRIBE, TRUE,  "List-Unsubscribe" },
	{ EMLA_ACTION_SUBSCRIBE,   FALSE, "List-Subscribe" },
	{ EMLA_ACTION_POST,        TRUE,  "List-Post" },
	{ EMLA_ACTION_OWNER,       TRUE,  "List-Owner" },
	{ EMLA_ACTION_ARCHIVE,     FALSE, "List-Archive" },
};

const gint emla_n_action_headers = sizeof(emla_action_headers) / sizeof(EmlaActionHeader);

void emla_list_action (EPlugin *item, EMMenuTargetSelect* sel, EmlaAction action);
void emla_list_help (EPlugin *item, EMMenuTargetSelect* sel);
void emla_list_unsubscribe (EPlugin *item, EMMenuTargetSelect* sel);
void emla_list_subscribe (EPlugin *item, EMMenuTargetSelect* sel);
void emla_list_post (EPlugin *item, EMMenuTargetSelect* sel);
void emla_list_owner (EPlugin *item, EMMenuTargetSelect* sel);
void emla_list_archive (EPlugin *item, EMMenuTargetSelect* sel);

void emla_list_action_do (CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer data);

typedef struct {
	EmlaAction action;
	gchar * uri;
} emla_action_data;

void emla_list_action (EPlugin *item, EMMenuTargetSelect* sel, EmlaAction action)
{
	emla_action_data *data;

	g_return_if_fail (sel->uids->len == 1);

	data = (emla_action_data *) malloc (sizeof (emla_action_data));
	data->action = action;
	data->uri = strdup (sel->uri);

	mail_get_message (sel->folder, (const gchar *) g_ptr_array_index (sel->uids, 0),
			emla_list_action_do, data, mail_msg_unordered_push);
}

void emla_list_action_do (CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer data)
{
	emla_action_data *action_data = (emla_action_data *) data;
	EmlaAction action = action_data->action;
	const gchar * header = NULL, *headerpos;
	gchar *end, *url = NULL;
	gint t;
	EMsgComposer *composer;
	gint send_message_response;
	EAccount *account;

	if (msg == NULL)
		return;

	for (t = 0; t < emla_n_action_headers; t++) {
		if (emla_action_headers[t].action == action &&
		    (header = camel_medium_get_header (CAMEL_MEDIUM (msg), emla_action_headers[t].header)) != NULL)
			break;
	}

	if (!header) {
		/* there was no header matching the action */
		e_error_run (NULL, "org.gnome.mailing-list-actions:no-header", NULL);
		goto exit;
	}

	headerpos = header;

	if (action == EMLA_ACTION_POST) {
		while (*headerpos == ' ') headerpos++;
		if (g_ascii_strcasecmp (headerpos, "NO") == 0) {
			e_error_run (NULL, "org.gnome.mailing-list-actions:posting-not-allowed", NULL);
			goto exit;
		}
	}

	/* parse the action value */
	while (*headerpos) {
		/* skip whitespace */
		while (*headerpos == ' ') headerpos++;
		if (*headerpos != '<' || (end = strchr (headerpos++, '>')) == NULL) {
			e_error_run (NULL, "org.gnome.mailing-list-actions:malformed-header", emla_action_headers[t].header, header, NULL);
			goto exit;
		}

		/* get URL portion */
		url = g_strndup(headerpos, end - headerpos);

		if (strncmp (url, "mailto:", 6) == 0) {
			if (emla_action_headers[t].interactive)
				send_message_response = GTK_RESPONSE_NO;
			else
				send_message_response = e_error_run (NULL, "org.gnome.mailing-list-actions:ask-send-message", url, NULL);

			if (send_message_response == GTK_RESPONSE_YES) {
				/* directly send message */
				composer = e_msg_composer_new_from_url (url);
				if ((account = mail_config_get_account_by_source_url (action_data->uri)))
					e_composer_header_table_set_account (
						e_msg_composer_get_header_table (composer),
						account);
				em_utils_composer_send_cb (composer, NULL);
			} else if (send_message_response == GTK_RESPONSE_NO) {
				/* show composer */
				em_utils_compose_new_message_with_mailto (url, action_data->uri);
			}

			goto exit;
		} else {
			/* FIXME Pass a parent window. */
			e_show_uri (NULL, url);
			goto exit;
		}
		g_free (url);
		url = NULL;
		headerpos = end++;

		/* ignore everything 'till next comma */
		headerpos = strchr (headerpos, ',');
		if (!headerpos)
			break;
		headerpos++;
	}

	/* if we got here, there's no valid action */
	e_error_run (NULL, "org.gnome.mailing-list-actions:no-action", header, NULL);

exit:
	free (action_data->uri);
	free (action_data);
	g_free(url);
}

void emla_list_help (EPlugin *item, EMMenuTargetSelect* sel)
{
	emla_list_action (item, sel, EMLA_ACTION_HELP);
}

void emla_list_unsubscribe (EPlugin *item, EMMenuTargetSelect* sel)
{
	emla_list_action (item, sel, EMLA_ACTION_UNSUBSCRIBE);
}

void emla_list_subscribe (EPlugin *item, EMMenuTargetSelect* sel)
{
	emla_list_action (item, sel, EMLA_ACTION_SUBSCRIBE);
}

void emla_list_post (EPlugin *item, EMMenuTargetSelect* sel)
{
	emla_list_action (item, sel, EMLA_ACTION_POST);
}

void emla_list_owner (EPlugin *item, EMMenuTargetSelect* sel)
{
	emla_list_action (item, sel, EMLA_ACTION_OWNER);
}

void emla_list_archive (EPlugin *item, EMMenuTargetSelect* sel)
{
	emla_list_action (item, sel, EMLA_ACTION_ARCHIVE);
}
