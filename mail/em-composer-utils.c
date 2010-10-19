/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include <libedataserver/e-data-server-util.h>
#include <glib/gi18n.h>

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"
#include "mail-session.h"
#include "mail-send-recv.h"

#include "e-util/e-account-utils.h"
#include "e-util/e-alert-dialog.h"
#include "e-util/e-util.h"

#include "shell/e-shell.h"

#include "e-mail-local.h"
#include "em-utils.h"
#include "em-composer-utils.h"
#include "composer/e-msg-composer.h"
#include "composer/e-composer-actions.h"
#include "composer/e-composer-post-header.h"
#include "em-folder-selector.h"
#include "em-folder-tree.h"
#include "em-format-html.h"
#include "em-format-html-print.h"
#include "em-format-quote.h"
#include "em-event.h"

#ifdef G_OS_WIN32
#ifdef gmtime_r
#undef gmtime_r
#endif

/* The gmtime() in Microsoft's C library is MT-safe */
#define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
#endif

#define GCONF_KEY_TEMPLATE_PLACEHOLDERS "/apps/evolution/mail/template_placeholders"

static void em_utils_composer_send_cb (EMsgComposer *composer);
static void em_utils_composer_save_draft_cb (EMsgComposer *composer);

struct emcs_t {
	guint ref_count;

	CamelFolder *drafts_folder;
	gchar *drafts_uid;

	CamelFolder *folder;
	guint32 flags, set;
	gchar *uid;
};

static struct emcs_t *
emcs_new (void)
{
	struct emcs_t *emcs;

	emcs = g_new0 (struct emcs_t, 1);
	emcs->ref_count = 1;

	return emcs;
}

static void
emcs_set_drafts_info (struct emcs_t *emcs,
                      CamelFolder *drafts_folder,
                      const gchar *drafts_uid)
{
	g_return_if_fail (emcs != NULL);
	g_return_if_fail (drafts_folder != NULL);
	g_return_if_fail (drafts_uid != NULL);

	if (emcs->drafts_folder != NULL)
		g_object_unref (emcs->drafts_folder);
	g_free (emcs->drafts_uid);

	g_object_ref (drafts_folder);
	emcs->drafts_folder = drafts_folder;
	emcs->drafts_uid = g_strdup (drafts_uid);
}

static void
emcs_set_folder_info (struct emcs_t *emcs,
                      CamelFolder *folder,
                      const gchar *uid,
                      guint32 flags,
                      guint32 set)
{
	g_return_if_fail (emcs != NULL);
	g_return_if_fail (folder != NULL);
	g_return_if_fail (uid != NULL);

	if (emcs->folder != NULL)
		g_object_unref (emcs->folder);
	g_free (emcs->uid);

	g_object_ref (folder);
	emcs->folder = folder;
	emcs->uid = g_strdup (uid);
	emcs->flags = flags;
	emcs->set = set;
}

static void
free_emcs (struct emcs_t *emcs)
{
	if (emcs->drafts_folder != NULL)
		g_object_unref (emcs->drafts_folder);
	g_free (emcs->drafts_uid);

	if (emcs->folder != NULL)
		g_object_unref (emcs->folder);
	g_free (emcs->uid);

	g_free (emcs);
}

static void
emcs_ref (struct emcs_t *emcs)
{
	emcs->ref_count++;
}

static void
emcs_unref (struct emcs_t *emcs)
{
	emcs->ref_count--;
	if (emcs->ref_count == 0)
		free_emcs (emcs);
}

static gboolean
ask_confirm_for_unwanted_html_mail (EMsgComposer *composer, EDestination **recipients)
{
	gboolean res;
	GString *str;
	gint i;

	str = g_string_new("");
	for (i = 0; recipients[i] != NULL; ++i) {
		if (!e_destination_get_html_mail_pref (recipients[i])) {
			const gchar *name;

			name = e_destination_get_textrep (recipients[i], FALSE);

			g_string_append_printf (str, "     %s\n", name);
		}
	}

	if (str->len)
		res = em_utils_prompt_user((GtkWindow *)composer,"/apps/evolution/mail/prompts/unwanted_html",
					   "mail:ask-send-html", str->str, NULL);
	else
		res = TRUE;

	g_string_free(str, TRUE);

	return res;
}

static gboolean
ask_confirm_for_empty_subject (EMsgComposer *composer)
{
	return em_utils_prompt_user((GtkWindow *)composer, "/apps/evolution/mail/prompts/empty_subject",
				    "mail:ask-send-no-subject", NULL);
}

static gboolean
ask_confirm_for_only_bcc (EMsgComposer *composer, gboolean hidden_list_case)
{
	/* If the user is mailing a hidden contact list, it is possible for
	   them to create a message with only Bcc recipients without really
	   realizing it.  To try to avoid being totally confusing, I've changed
	   this dialog to provide slightly different text in that case, to
	   better explain what the hell is going on. */

	return em_utils_prompt_user((GtkWindow *)composer, "/apps/evolution/mail/prompts/only_bcc",
				    hidden_list_case?"mail:ask-send-only-bcc-contact":"mail:ask-send-only-bcc", NULL);
}

struct _send_data {
	struct emcs_t *emcs;
	EMsgComposer *composer;
	gboolean send;
};

static void
composer_send_queued_cb (CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info,
			 gint queued, const gchar *appended_uid, gpointer data)
{
	struct emcs_t *emcs;
	struct _send_data *send = data;

	emcs = send->emcs;

	if (queued) {
		if (emcs && emcs->drafts_folder) {
			/* delete the old draft message */
			camel_folder_set_message_flags (emcs->drafts_folder, emcs->drafts_uid,
							CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN,
							CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN);
			g_object_unref (emcs->drafts_folder);
			emcs->drafts_folder = NULL;
			g_free (emcs->drafts_uid);
			emcs->drafts_uid = NULL;
		}

		if (emcs && emcs->folder) {
			/* set any replied flags etc */
			camel_folder_set_message_flags (emcs->folder, emcs->uid, emcs->flags, emcs->set);
			camel_folder_set_message_user_flag (emcs->folder, emcs->uid, "receipt-handled", TRUE);
			g_object_unref (emcs->folder);
			emcs->folder = NULL;
			g_free (emcs->uid);
			emcs->uid = NULL;
		}

		gtk_widget_destroy (GTK_WIDGET (send->composer));

		if (send->send && camel_session_get_online (session)) {
			/* queue a message send */
			mail_send ();
		}
	} else
		gtk_widget_show (GTK_WIDGET (send->composer));

	camel_message_info_free (info);

	if (send->emcs)
		emcs_unref (send->emcs);

	g_object_unref (send->composer);
	g_free (send);
}

static gboolean
is_group_definition (const gchar *str)
{
	const gchar *colon;

	if (!str || !*str)
		return FALSE;

	colon = strchr (str, ':');
	return colon > str && strchr (str, ';') > colon;
}

static CamelMimeMessage *
composer_get_message (EMsgComposer *composer, gboolean save_html_object_data)
{
	CamelMimeMessage *message = NULL;
	EDestination **recipients, **recipients_bcc;
	gboolean html_mode, send_html, confirm_html;
	CamelInternetAddress *cia;
	gint hidden = 0, shown = 0;
	gint num = 0, num_bcc = 0, num_post = 0;
	const gchar *subject;
	GConfClient *gconf;
	EAccount *account;
	gint i;
	EMEvent *eme;
	EMEventTargetComposer *target;
	EComposerHeaderTable *table;
	EComposerHeader *post_to_header;
	GString *invalid_addrs = NULL;
	GError *error = NULL;

	gconf = mail_config_get_gconf_client ();
	table = e_msg_composer_get_header_table (composer);

	/* We should do all of the validity checks based on the composer, and not on
	   the created message, as extra interaction may occur when we get the message
	   (e.g. to get a passphrase to sign a message) */

	/* get the message recipients */
	recipients = e_composer_header_table_get_destinations (table);

	cia = camel_internet_address_new ();

	/* see which ones are visible/present, etc */
	if (recipients) {
		for (i = 0; recipients[i] != NULL; i++) {
			const gchar *addr = e_destination_get_address (recipients[i]);

			if (addr && addr[0]) {
				gint len, j;

				camel_address_decode ((CamelAddress *) cia, addr);
				len = camel_address_length ((CamelAddress *) cia);

				if (len > 0) {
					if (!e_destination_is_evolution_list (recipients[i])) {
						for (j = 0; j < len; j++) {
							const gchar *name = NULL, *eml = NULL;

							if (!camel_internet_address_get (cia, j, &name, &eml) ||
							    !eml ||
							    strchr (eml, '@') <= eml) {
								if (!invalid_addrs)
									invalid_addrs = g_string_new ("");
								else
									g_string_append (invalid_addrs, ", ");

								if (name)
									g_string_append (invalid_addrs, name);
								if (eml) {
									g_string_append (invalid_addrs, name ? " <" : "");
									g_string_append (invalid_addrs, eml);
									g_string_append (invalid_addrs, name ? ">" : "");
								}
							}
						}
					}

					camel_address_remove ((CamelAddress *) cia, -1);
					num++;
					if (e_destination_is_evolution_list (recipients[i])
					    && !e_destination_list_show_addresses (recipients[i])) {
						hidden++;
					} else {
						shown++;
					}
				} else if (is_group_definition (addr)) {
					/* like an address, it will not claim on only-bcc */
					shown++;
					num++;
				} else if (!invalid_addrs) {
					invalid_addrs = g_string_new (addr);
				} else {
					g_string_append (invalid_addrs, ", ");
					g_string_append (invalid_addrs, addr);
				}
			}
		}
	}

	recipients_bcc = e_composer_header_table_get_destinations_bcc (table);
	if (recipients_bcc) {
		for (i = 0; recipients_bcc[i] != NULL; i++) {
			const gchar *addr = e_destination_get_address (recipients_bcc[i]);

			if (addr && addr[0]) {
				camel_address_decode ((CamelAddress *) cia, addr);
				if (camel_address_length ((CamelAddress *) cia) > 0) {
					camel_address_remove ((CamelAddress *) cia, -1);
					num_bcc++;
				}
			}
		}

		e_destination_freev (recipients_bcc);
	}

	g_object_unref (cia);

	post_to_header = e_composer_header_table_get_header (table, E_COMPOSER_HEADER_POST_TO);
	if (e_composer_header_get_visible (post_to_header)) {
		GList *postlist;

		postlist = e_composer_header_table_get_post_to (table);
		num_post = g_list_length (postlist);
		g_list_foreach (postlist, (GFunc)g_free, NULL);
		g_list_free (postlist);
	}

	/* I'm sensing a lack of love, er, I mean recipients. */
	if (num == 0 && num_post == 0) {
		e_alert_run_dialog_for_args ((GtkWindow *)composer, "mail:send-no-recipients", NULL);
		goto finished;
	}

	if (invalid_addrs) {
		if (e_alert_run_dialog_for_args ((GtkWindow *)composer, strstr (invalid_addrs->str, ", ") ? "mail:ask-send-invalid-recip-multi" : "mail:ask-send-invalid-recip-one", invalid_addrs->str, NULL) == GTK_RESPONSE_CANCEL) {
			g_string_free (invalid_addrs, TRUE);
			goto finished;
		}

		g_string_free (invalid_addrs, TRUE);
	}

	if (num > 0 && (num == num_bcc || shown == 0)) {
		/* this means that the only recipients are Bcc's */
		if (!ask_confirm_for_only_bcc (composer, shown == 0))
			goto finished;
	}

	html_mode = gtkhtml_editor_get_html_mode (GTKHTML_EDITOR (composer));
	send_html = gconf_client_get_bool (gconf, "/apps/evolution/mail/composer/send_html", NULL);
	confirm_html = gconf_client_get_bool (gconf, "/apps/evolution/mail/prompts/unwanted_html", NULL);

	/* Only show this warning if our default is to send html.  If it isn't, we've
	   manually switched into html mode in the composer and (presumably) had a good
	   reason for doing this. */
	if (html_mode && send_html && confirm_html) {

		gboolean html_problem = FALSE;

		if (recipients) {
			for (i = 0; recipients[i] != NULL && !html_problem; i++) {
				if (!e_destination_get_html_mail_pref (recipients[i]))
					html_problem = TRUE;
			}
		}

		if (html_problem) {
			html_problem = !ask_confirm_for_unwanted_html_mail (composer, recipients);
			if (html_problem)
				goto finished;
		}
	}

	/* Check for no subject */
	subject = e_composer_header_table_get_subject (table);
	if (subject == NULL || subject[0] == '\0') {
		if (!ask_confirm_for_empty_subject (composer))
			goto finished;
	}

	/** @Event: composer.presendchecks
	 * @Title: Composer PreSend Checks
	 * @Target: EMEventTargetMessage
	 *
	 * composer.presendchecks is emitted during pre-checks for the message just before sending.
	 * Since the e-plugin framework doesn't provide a way to return a value from the plugin,
	 * use 'presend_check_status' to set whether the check passed / failed.
	 */
	eme = em_event_peek();
	target = em_event_target_new_composer (eme, composer, 0);
	g_object_set_data (G_OBJECT (composer), "presend_check_status", GINT_TO_POINTER(0));

	e_event_emit((EEvent *)eme, "composer.presendchecks", (EEventTarget *)target);

	if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (composer), "presend_check_status")))
		goto finished;

	/* actually get the message now, this will sign/encrypt etc */
	message = e_msg_composer_get_message (
		composer, save_html_object_data, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (message == NULL);
		g_error_free (error);
		goto finished;
	}

	if (error != NULL) {
		g_warn_if_fail (message == NULL);
		e_alert_run_dialog_for_args (
			GTK_WINDOW (composer),
			"mail-composer:no-build-message",
			error->message, NULL);
		g_error_free (error);
		goto finished;
	}

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	/* Add info about the sending account */
	account = e_composer_header_table_get_account (table);

	if (account) {
		/* FIXME: Why isn't this crap just in e_msg_composer_get_message? */
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Account", account->uid);
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Transport", account->transport->url);
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Fcc", account->sent_folder_uri);
		if (account->id->organization && *account->id->organization) {
			gchar *org;

			org = camel_header_encode_string ((const guchar *)account->id->organization);
			camel_medium_set_header (CAMEL_MEDIUM (message), "Organization", org);
			g_free (org);
		}
	}

 finished:

	if (recipients)
		e_destination_freev (recipients);

	return message;
}

static void
em_utils_composer_send_cb (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	struct _send_data *send;
	CamelFolder *folder;
	EAccount *account;

	table = e_msg_composer_get_header_table (composer);
	account = e_composer_header_table_get_account (table);
	if (!account || !account->enabled) {
		e_alert_run_dialog_for_args (
			GTK_WINDOW (composer),
			"mail:send-no-account-enabled", NULL);
		return;
	}

	if ((message = composer_get_message (composer, FALSE)) == NULL)
		return;

	folder = e_mail_local_get_folder (E_MAIL_FOLDER_OUTBOX);
	g_object_ref (folder);

	/* mail the message */
	e_msg_composer_set_mail_sent (composer, TRUE);
	info = camel_message_info_new (NULL);
	camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, ~0);

	send = g_malloc (sizeof (*send));
	send->emcs = g_object_get_data (G_OBJECT (composer), "emcs");
	if (send->emcs)
		emcs_ref (send->emcs);
	send->send = TRUE;
	send->composer = g_object_ref (composer);
	gtk_widget_hide (GTK_WIDGET (composer));

	mail_append_mail (
		folder, message, info, composer_send_queued_cb, send);

	g_object_unref (folder);
	g_object_unref (message);
}

struct _save_draft_info {
	struct emcs_t *emcs;
	EMsgComposer *composer;
	CamelMessageInfo *info;
};

static void
composer_set_no_change (EMsgComposer *composer)
{
	GtkhtmlEditor *editor;

	g_return_if_fail (composer != NULL);

	editor = GTKHTML_EDITOR (composer);

	gtkhtml_editor_drop_undo (editor);
	gtkhtml_editor_set_changed (editor, FALSE);
}

static void
save_draft_done (CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, gint ok,
		 const gchar *appended_uid, gpointer user_data)
{
	struct _save_draft_info *sdi = user_data;
	struct emcs_t *emcs;

	if (!ok)
		goto done;

	if ((emcs = sdi->emcs) == NULL)
		emcs = emcs_new ();

	if (emcs->drafts_folder) {
		/* delete the original draft message */
		camel_folder_set_message_flags (emcs->drafts_folder, emcs->drafts_uid,
						CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN,
						CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN);
		g_object_unref (emcs->drafts_folder);
		emcs->drafts_folder = NULL;
		g_free (emcs->drafts_uid);
		emcs->drafts_uid = NULL;
	}

	if (emcs->folder) {
		/* set the replied flags etc */
		camel_folder_set_message_flags (emcs->folder, emcs->uid, emcs->flags, emcs->set);
		g_object_unref (emcs->folder);
		emcs->folder = NULL;
		g_free (emcs->uid);
		emcs->uid = NULL;
	}

	if (appended_uid) {
		g_object_ref (folder);
		emcs->drafts_folder = folder;
		emcs->drafts_uid = g_strdup (appended_uid);
	}

	if (e_msg_composer_is_exiting (sdi->composer))
		gtk_widget_destroy (GTK_WIDGET (sdi->composer));

 done:
	g_object_unref (sdi->composer);
	if (sdi->emcs)
		emcs_unref (sdi->emcs);
	camel_message_info_free(info);
	g_free (sdi);
}

static void
save_draft_folder (gchar *uri, CamelFolder *folder, gpointer data)
{
	CamelFolder **save = data;

	if (folder) {
		*save = folder;
		g_object_ref (folder);
	}
}

static void
em_utils_composer_save_draft_cb (EMsgComposer *composer)
{
	CamelFolder *local_drafts_folder;
	EComposerHeaderTable *table;
	struct _save_draft_info *sdi;
	const gchar *local_drafts_folder_uri;
	CamelFolder *folder = NULL;
	CamelMimeMessage *msg;
	CamelMessageInfo *info;
	EAccount *account;
	GError *error = NULL;

	/* need to get stuff from the composer here, since it could
	 * get destroyed while we're in mail_msg_wait() a little lower
	 * down, waiting for the folder to open */

	local_drafts_folder =
		e_mail_local_get_folder (E_MAIL_FOLDER_DRAFTS);
	local_drafts_folder_uri =
		e_mail_local_get_folder_uri (E_MAIL_FOLDER_DRAFTS);

	msg = e_msg_composer_get_message_draft (composer, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (msg == NULL);
		g_error_free (error);
		return;
	}

	if (error != NULL) {
		g_warn_if_fail (msg == NULL);
		e_alert_run_dialog_for_args (
			GTK_WINDOW (composer),
			"mail-composer:no-build-message",
			error->message, NULL);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (msg));

	table = e_msg_composer_get_header_table (composer);
	account = e_composer_header_table_get_account (table);

	sdi = g_malloc (sizeof(struct _save_draft_info));
	sdi->composer = g_object_ref (composer);
	sdi->emcs = g_object_get_data (G_OBJECT (composer), "emcs");
	if (sdi->emcs)
		emcs_ref (sdi->emcs);

	if (account && account->drafts_folder_uri &&
	    strcmp (account->drafts_folder_uri, local_drafts_folder_uri) != 0) {
		gint id;

		id = mail_get_folder (account->drafts_folder_uri, 0, save_draft_folder, &folder, mail_msg_unordered_push);
		mail_msg_wait (id);

		if (!folder || !account->enabled) {
			if (e_alert_run_dialog_for_args ((GtkWindow *)composer, "mail:ask-default-drafts", NULL) != GTK_RESPONSE_YES) {
				g_object_unref(composer);
				g_object_unref (msg);
				if (sdi->emcs)
					emcs_unref(sdi->emcs);
				g_free(sdi);
				return;
			}

			folder = local_drafts_folder;
			g_object_ref (local_drafts_folder);
		}
	} else {
		folder = local_drafts_folder;
		g_object_ref (folder);
	}

	info = camel_message_info_new(NULL);
	camel_message_info_set_flags(info, CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_SEEN, ~0);

	mail_append_mail (folder, msg, info, save_draft_done, sdi);
	g_object_unref (folder);
	g_object_unref (msg);
}

static void
em_utils_composer_print_cb (EMsgComposer *composer,
                            GtkPrintOperationAction action)
{
	CamelMimeMessage *message;
	EMFormatHTMLPrint *efhp;

	message = e_msg_composer_get_message_print (composer, 1);

	efhp = em_format_html_print_new (NULL, action);
	em_format_html_print_raw_message (efhp, message);
	g_object_unref (efhp);
}

/* Composing messages... */

static EMsgComposer *
create_new_composer (EShell *shell,
                     const gchar *subject,
                     const gchar *from_uri)
{
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	EAccount *account = NULL;

	composer = e_msg_composer_new (shell);

	table = e_msg_composer_get_header_table (composer);

	if (from_uri != NULL) {
		GList *list;

		account = mail_config_get_account_by_source_url(from_uri);

		list = g_list_prepend (NULL, (gpointer) from_uri);
		e_composer_header_table_set_post_to_list (table, list);
		g_list_free (list);
	}

	e_composer_header_table_set_account (table, account);
	e_composer_header_table_set_subject (table, subject);

	return composer;
}

/**
 * em_utils_compose_new_message:
 * @shell: an #EShell
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window.
 **/
void
em_utils_compose_new_message (EShell *shell,
                              const gchar *from_uri)
{
	GtkWidget *composer;

	g_return_if_fail (E_IS_SHELL (shell));

	composer = (GtkWidget *) create_new_composer (shell, "", from_uri);
	if (composer == NULL)
		return;

	composer_set_no_change (E_MSG_COMPOSER (composer));

	gtk_widget_show (composer);
}

/**
 * em_utils_compose_new_message_with_mailto:
 * @shell: an #EShell
 * @url: mailto url
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window. If @url is non-NULL, the composer fields will be filled in
 * according to the values in the mailto url.
 **/
EMsgComposer *
em_utils_compose_new_message_with_mailto (EShell *shell,
                                          const gchar *url,
                                          const gchar *from_uri)
{
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	EAccount *account = NULL;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	if (url != NULL)
		composer = e_msg_composer_new_from_url (shell, url);
	else
		composer = e_msg_composer_new (shell);

	table = e_msg_composer_get_header_table (composer);

	if (from_uri
	    && (account = mail_config_get_account_by_source_url(from_uri)))
		e_composer_header_table_set_account_name (table, account->name);

	composer_set_no_change (composer);

	gtk_window_present (GTK_WINDOW (composer));

	return composer;
}

static gboolean
replace_variables (GSList *clues, CamelMimeMessage *message, gchar **pstr)
{
	gint i;
	gboolean string_changed = FALSE, count1 = FALSE;
	gchar *str;

	g_return_val_if_fail (pstr != NULL, FALSE);
	g_return_val_if_fail (*pstr != NULL, FALSE);
	g_return_val_if_fail (message != NULL, FALSE);

	str = *pstr;

	for (i = 0; i < strlen (str); i++) {
		const gchar *cur = str + i;
		if (!g_ascii_strncasecmp (cur, "$", 1)) {
			const gchar *end = cur + 1;
			gchar *out;
			gchar **temp_str;
			GSList *list;

			while (*end && (g_unichar_isalnum (*end) || *end == '_'))
				end++;

			out = g_strndup ((const gchar *) cur, end - cur);

			temp_str = g_strsplit (str, out, 2);

			for (list = clues; list; list = g_slist_next (list)) {
				gchar **temp = g_strsplit (list->data, "=", 2);
				if (!g_ascii_strcasecmp (temp[0], out+1)) {
					g_free (str);
					str = g_strconcat (temp_str[0], temp[1], temp_str[1], NULL);
					count1 = TRUE;
					string_changed = TRUE;
				} else
					count1 = FALSE;
				g_strfreev(temp);
			}

			if (!count1) {
				if (getenv (out+1)) {
					g_free (str);
					str = g_strconcat (temp_str[0], getenv (out + 1), temp_str[1], NULL);
					count1 = TRUE;
					string_changed = TRUE;
				} else
					count1 = FALSE;
			}

			if (!count1) {
				CamelInternetAddress *to;
				const gchar *name, *addr;

				to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
				if (!camel_internet_address_get (to, 0, &name, &addr))
					continue;

				if (name && g_ascii_strcasecmp ("sender_name", out + 1) == 0) {
					g_free (str);
					str = g_strconcat (temp_str[0], name, temp_str[1], NULL);
					count1 = TRUE;
					string_changed = TRUE;
				} else if (addr && g_ascii_strcasecmp ("sender_email", out + 1) == 0) {
					g_free (str);
					str = g_strconcat (temp_str[0], addr, temp_str[1], NULL);
					count1 = TRUE;
					string_changed = TRUE;
				}
			}

			g_strfreev (temp_str);
			g_free (out);
		}
	}

	*pstr = str;

	return string_changed;
}

static void
traverse_parts (GSList *clues, CamelMimeMessage *message, CamelDataWrapper *content)
{
	g_return_if_fail (message != NULL);

	if (!content)
		return;

	if (CAMEL_IS_MULTIPART (content)) {
		guint i, n;
		CamelMultipart *multipart = CAMEL_MULTIPART (content);
		CamelMimePart *part;

		n = camel_multipart_get_number (multipart);
		for (i = 0; i < n; i++) {
			part = camel_multipart_get_part (multipart, i);
			if (!part)
				continue;

			traverse_parts (clues, message, CAMEL_DATA_WRAPPER (part));
		}
	} else if (CAMEL_IS_MIME_PART (content)) {
		CamelMimePart *part = CAMEL_MIME_PART (content);
		CamelContentType *type;
		CamelStream *stream;
		GByteArray *byte_array;
		gchar *str;

		content = camel_medium_get_content (CAMEL_MEDIUM (part));
		if (!content)
			return;

		if (CAMEL_IS_MULTIPART (content)) {
			traverse_parts (clues, message, CAMEL_DATA_WRAPPER (content));
			return;
		}

		type = camel_mime_part_get_content_type (part);
		if (!camel_content_type_is (type, "text", "*"))
			return;

		byte_array = g_byte_array_new ();
		stream = camel_stream_mem_new_with_byte_array (byte_array);
		camel_data_wrapper_decode_to_stream (content, stream, NULL);

		str = g_strndup ((gchar *) byte_array->data, byte_array->len);
		g_object_unref (stream);

		if (replace_variables (clues, message, &str)) {
			stream = camel_stream_mem_new_with_buffer (str, strlen (str));
			camel_stream_reset (stream, NULL);
			camel_data_wrapper_construct_from_stream (content, stream, NULL);
			g_object_unref (stream);
		}

		g_free (str);
	}
}

/* Editing messages... */

static GtkWidget *
edit_message (EShell *shell,
              CamelMimeMessage *message,
              CamelFolder *drafts,
              const gchar *uid)
{
	EMsgComposer *composer;

	/* Template specific code follows. */
	if (em_utils_folder_is_templates (drafts, NULL) == TRUE) {
		GConfClient *gconf;
		GSList *clue_list = NULL;

		gconf = gconf_client_get_default ();
		/* Get the list from gconf */
		clue_list = gconf_client_get_list ( gconf, GCONF_KEY_TEMPLATE_PLACEHOLDERS, GCONF_VALUE_STRING, NULL );
		g_object_unref (gconf);

		traverse_parts (clue_list, message, camel_medium_get_content (CAMEL_MEDIUM (message)));

		g_slist_foreach (clue_list, (GFunc) g_free, NULL);
		g_slist_free (clue_list);
	}

	composer = e_msg_composer_new_with_message (shell, message);

	if (em_utils_folder_is_drafts (drafts, NULL)) {
		struct emcs_t *emcs;

		emcs = g_object_get_data (G_OBJECT (composer), "emcs");
		emcs_set_drafts_info (emcs, drafts, uid);
	}

	composer_set_no_change (composer);

	gtk_widget_show (GTK_WIDGET (composer));

	return (GtkWidget *)composer;
}

/**
 * em_utils_edit_message:
 * @shell: an #EShell
 * @message: message to edit
 * @folder: used to recognize the templates folder
 *
 * Opens a composer filled in with the headers/mime-parts/etc of
 * @message.
 **/
GtkWidget *
em_utils_edit_message (EShell *shell,
                       CamelMimeMessage *message,
                       CamelFolder *folder)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	return edit_message (shell, message, folder, NULL);
}

static void
edit_messages_replace (CamelFolder *folder,
                       GPtrArray *uids,
                       GPtrArray *msgs,
                       gpointer user_data)
{
	EShell *shell = E_SHELL (user_data);
	gint ii;

	if (msgs == NULL)
		return;

	for (ii = 0; ii < msgs->len; ii++) {
		camel_medium_remove_header (
			CAMEL_MEDIUM (msgs->pdata[ii]), "X-Mailer");
		edit_message (shell, msgs->pdata[ii], folder, uids->pdata[ii]);
	}

	g_object_unref (shell);
}

static void
edit_messages_no_replace (CamelFolder *folder,
                          GPtrArray *uids,
                          GPtrArray *msgs,
                          gpointer user_data)
{
	EShell *shell = E_SHELL (user_data);
	gint ii;

	if (msgs == NULL)
		return;

	for (ii = 0; ii < msgs->len; ii++) {
		camel_medium_remove_header (
			CAMEL_MEDIUM (msgs->pdata[ii]), "X-Mailer");
		edit_message (shell, msgs->pdata[ii], NULL, NULL);
	}

	g_object_unref (shell);
}

/**
 * em_utils_edit_messages:
 * @shell: an #EShell
 * @folder: folder containing messages to edit
 * @uids: uids of messages to edit
 * @replace: replace the existing message(s) when sent or saved.
 *
 * Opens a composer for each message to be edited.
 **/
void
em_utils_edit_messages (EShell *shell,
                        CamelFolder *folder,
                        GPtrArray *uids,
                        gboolean replace)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	if (replace)
		mail_get_messages (
			folder, uids, edit_messages_replace,
			g_object_ref (shell));
	else
		mail_get_messages (
			folder, uids, edit_messages_no_replace,
			g_object_ref (shell));
}

static void
emu_update_composers_security (EMsgComposer *composer, guint32 validity_found)
{
	GtkToggleAction *action;

	g_return_if_fail (composer != NULL);

	/* Pre-set only for encrypted messages, not for signed */
	/*if (validity_found & EM_FORMAT_VALIDITY_FOUND_SIGNED) {
		if (validity_found & EM_FORMAT_VALIDITY_FOUND_SMIME)
			action = GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_SMIME_SIGN (composer));
		else
			action = GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_PGP_SIGN (composer));
		gtk_toggle_action_set_active (action, TRUE);
	}*/

	if (validity_found & EM_FORMAT_VALIDITY_FOUND_ENCRYPTED) {
		if (validity_found & EM_FORMAT_VALIDITY_FOUND_SMIME)
			action = GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_SMIME_ENCRYPT (composer));
		else
			action = GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_PGP_ENCRYPT (composer));
		gtk_toggle_action_set_active (action, TRUE);
	}
}

/* Forwarding messages... */
struct forward_attached_data
{
	EShell *shell;
	CamelFolder *folder;
	GPtrArray *uids;
	gchar *from_uri;
};

static void
real_update_forwarded_flag (gpointer uid, gpointer folder)
{
	if (uid && folder)
		camel_folder_set_message_flags (folder, uid, CAMEL_MESSAGE_FORWARDED, CAMEL_MESSAGE_FORWARDED);
}

static void
update_forwarded_flags_cb (EMsgComposer *composer, gpointer user_data)
{
	struct forward_attached_data *fad = (struct forward_attached_data *) user_data;

	if (fad && fad->uids && fad->folder)
		g_ptr_array_foreach (fad->uids, real_update_forwarded_flag, fad->folder);
}

static void
composer_destroy_fad_cb (gpointer user_data, GObject *deadbeef)
{
	struct forward_attached_data *fad = (struct forward_attached_data*) user_data;

	if (fad) {
		g_object_unref (fad->folder);
		em_utils_uids_free (fad->uids);
		g_free (fad);
	}
}

static void
setup_forward_attached_callbacks (EMsgComposer *composer, CamelFolder *folder, GPtrArray *uids)
{
	struct forward_attached_data *fad;

	if (!composer || !folder || !uids || !uids->len)
		return;

	g_object_ref (folder);

	fad = g_new0 (struct forward_attached_data, 1);
	fad->folder = folder;
	fad->uids = em_utils_uids_copy (uids);

	g_signal_connect (composer, "send", G_CALLBACK (update_forwarded_flags_cb), fad);
	g_signal_connect (composer, "save-draft", G_CALLBACK (update_forwarded_flags_cb), fad);

	g_object_weak_ref ((GObject *) composer, (GWeakNotify) composer_destroy_fad_cb, fad);
}

static EMsgComposer *
forward_attached (EShell *shell,
                  CamelFolder *folder,
                  GPtrArray *uids,
                  GPtrArray *messages,
                  CamelMimePart *part,
                  gchar *subject,
                  const gchar *from_uri)
{
	EMsgComposer *composer;

	composer = create_new_composer (shell, subject, from_uri);
	if (composer == NULL)
		return NULL;

	e_msg_composer_attach (composer, part);

	if (uids)
		setup_forward_attached_callbacks (composer, folder, uids);

	composer_set_no_change (composer);

	gtk_widget_show (GTK_WIDGET (composer));

	return composer;
}

static void
forward_attached_cb (CamelFolder *folder,
                     GPtrArray *messages,
                     CamelMimePart *part,
                     gchar *subject,
                     gpointer user_data)
{
	struct forward_attached_data *fad = user_data;

	if (part)
		forward_attached (
			fad->shell, folder, fad->uids,
			messages, part, subject, fad->from_uri);

	g_object_unref (fad->shell);
	g_free (fad->from_uri);
	g_free (fad);
}

/**
 * em_utils_forward_attached:
 * @shell: an #EShell
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 * @from_uri: from folder uri
 *
 * If there is more than a single message in @uids, a multipart/digest
 * will be constructed and attached to a new composer window preset
 * with the appropriate header defaults for forwarding the first
 * message in the list. If only one message is to be forwarded, it is
 * forwarded as a simple message/rfc822 attachment.
 **/
void
em_utils_forward_attached (EShell *shell,
                           CamelFolder *folder,
                           GPtrArray *uids,
                           const gchar *from_uri)
{
	struct forward_attached_data *fad;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	fad = g_new0 (struct forward_attached_data, 1);
	fad->shell = g_object_ref (shell);
	fad->uids = uids;
	fad->from_uri = g_strdup (from_uri);

	mail_build_attachment (folder, uids, forward_attached_cb, fad);
}

static EMsgComposer *
forward_non_attached (EShell *shell,
                      CamelFolder *folder,
                      GPtrArray *uids,
                      GPtrArray *messages,
                      gint style,
                      const gchar *from_uri)
{
	CamelMimeMessage *message;
	EMsgComposer *composer = NULL;
	gchar *subject, *text;
	gint i;
	guint32 flags;

	if (messages->len == 0)
		return NULL;

	flags = EM_FORMAT_QUOTE_HEADERS | EM_FORMAT_QUOTE_KEEP_SIG;
	if (style == MAIL_CONFIG_FORWARD_QUOTED)
		flags |= EM_FORMAT_QUOTE_CITE;

	for (i = 0; i < messages->len; i++) {
		gssize len;
		guint32 validity_found = 0;

		message = messages->pdata[i];
		subject = mail_tool_generate_forward_subject (message);

		text = em_utils_message_to_html (message, _("-------- Forwarded Message --------"), flags, &len, NULL, NULL, &validity_found);

		if (text) {
			composer = create_new_composer (shell, subject, from_uri);

			if (composer) {
				if (CAMEL_IS_MULTIPART(camel_medium_get_content ((CamelMedium *)message)))
					e_msg_composer_add_message_attachments(composer, message, FALSE);

				e_msg_composer_set_body_text (composer, text, len);

				if (uids && uids->pdata[i]) {
					struct emcs_t *emcs;

					emcs = g_object_get_data (G_OBJECT (composer), "emcs");
					emcs_set_folder_info (emcs, folder, uids->pdata[i], CAMEL_MESSAGE_FORWARDED, CAMEL_MESSAGE_FORWARDED);
				}

				emu_update_composers_security (composer, validity_found);
				composer_set_no_change (composer);
				gtk_widget_show (GTK_WIDGET (composer));
			}
			g_free (text);
		}

		g_free (subject);
	}

	return composer;
}

typedef struct {
	EShell *shell;
	gchar *from_uri;
} ForwardData;

static void
forward_inline_cb (CamelFolder *folder,
                   GPtrArray *uids,
                   GPtrArray *messages,
                   gpointer user_data)
{
	ForwardData *data = user_data;

	forward_non_attached (
		data->shell, folder, uids, messages,
		MAIL_CONFIG_FORWARD_INLINE, data->from_uri);

	g_free (data->from_uri);
	g_object_unref (data->shell);
	g_slice_free (ForwardData, data);
}

/**
 * em_utils_forward_inline:
 * @shell: an #EShell
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 * @from_uri: from folder/account uri
 *
 * Forwards each message in the 'inline' form, each in its own composer window.
 **/
void
em_utils_forward_inline (EShell *shell,
                         CamelFolder *folder,
                         GPtrArray *uids,
                         const gchar *from_uri)
{
	ForwardData *data;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	data = g_slice_new (ForwardData);
	data->shell = g_object_ref (shell);
	data->from_uri = g_strdup (from_uri);

	mail_get_messages (folder, uids, forward_inline_cb, data);
}

static void
forward_quoted_cb (CamelFolder *folder,
                   GPtrArray *uids,
                   GPtrArray *messages,
                   gpointer user_data)
{
	ForwardData *data = user_data;

	forward_non_attached (
		data->shell, folder, uids, messages,
		MAIL_CONFIG_FORWARD_QUOTED, data->from_uri);

	g_free (data->from_uri);
	g_object_unref (data->shell);
	g_slice_free (ForwardData, data);
}

/**
 * em_utils_forward_quoted:
 * @shell: an #EShell
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 * @from_uri: from folder uri
 *
 * Forwards each message in the 'quoted' form (each line starting with
 * a "> "), each in its own composer window.
 **/
void
em_utils_forward_quoted (EShell *shell,
                         CamelFolder *folder,
                         GPtrArray *uids,
                         const gchar *from_uri)
{
	ForwardData *data;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	data = g_slice_new (ForwardData);
	data->shell = g_object_ref (shell);
	data->from_uri = g_strdup (from_uri);

	mail_get_messages (folder, uids, forward_quoted_cb, data);
}

/**
 * em_utils_forward_message:
 * @shell: an #EShell
 * @message: message to be forwarded
 * @from_uri: from folder uri
 *
 * Forwards a message in the user's configured default style.
 **/
EMsgComposer *
em_utils_forward_message (EShell *shell,
                          CamelMimeMessage *message,
                          const gchar *from_uri)
{
	GPtrArray *messages;
	CamelMimePart *part;
	GConfClient *gconf;
	gchar *subject;
	gint mode;
	EMsgComposer *composer = NULL;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	messages = g_ptr_array_new ();
	g_ptr_array_add (messages, message);

	gconf = mail_config_get_gconf_client ();
	mode = gconf_client_get_int (gconf, "/apps/evolution/mail/format/forward_style", NULL);

	switch (mode) {
		case MAIL_CONFIG_FORWARD_ATTACHED:
		default:
			part = mail_tool_make_message_attachment (message);
			subject = mail_tool_generate_forward_subject (message);

			composer = forward_attached (
				shell, NULL, NULL, messages,
				part, subject, from_uri);

			g_object_unref (part);
			g_free (subject);
			break;

		case MAIL_CONFIG_FORWARD_INLINE:
			composer = forward_non_attached (
				shell, NULL, NULL, messages,
				MAIL_CONFIG_FORWARD_INLINE, from_uri);
			break;

		case MAIL_CONFIG_FORWARD_QUOTED:
			composer = forward_non_attached (
				shell, NULL, NULL, messages,
				MAIL_CONFIG_FORWARD_QUOTED, from_uri);
			break;
	}

	g_ptr_array_free (messages, TRUE);

	return composer;
}

/**
 * em_utils_forward_messages:
 * @shell: an #EShell
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 *
 * Forwards a group of messages in the user's configured default
 * style.
 **/
void
em_utils_forward_messages (EShell *shell,
                           CamelFolder *folder,
                           GPtrArray *uids,
                           const gchar *from_uri)
{
	GConfClient *gconf;
	gint mode;

	g_return_if_fail (E_IS_SHELL (shell));

	gconf = mail_config_get_gconf_client ();
	mode = gconf_client_get_int (gconf, "/apps/evolution/mail/format/forward_style", NULL);

	switch (mode) {
		case MAIL_CONFIG_FORWARD_ATTACHED:
		default:
			em_utils_forward_attached (shell, folder, uids, from_uri);
			break;
		case MAIL_CONFIG_FORWARD_INLINE:
			em_utils_forward_inline (shell, folder, uids, from_uri);
			break;
		case MAIL_CONFIG_FORWARD_QUOTED:
			em_utils_forward_quoted (shell, folder, uids, from_uri);
			break;
	}
}

/* Redirecting messages... */

static EMsgComposer *
redirect_get_composer (EShell *shell,
                       CamelMimeMessage *message)
{
	EMsgComposer *composer;
	EAccount *account;

	/* QMail will refuse to send a message if it finds one of
	   it's Delivered-To headers in the message, so remove all
	   Delivered-To headers. Fixes bug #23635. */
	while (camel_medium_get_header (CAMEL_MEDIUM (message), "Delivered-To"))
		camel_medium_remove_header (CAMEL_MEDIUM (message), "Delivered-To");

	while (camel_medium_get_header (CAMEL_MEDIUM (message), "Bcc"))
		camel_medium_remove_header (CAMEL_MEDIUM (message), "Bcc");

	while (camel_medium_get_header (CAMEL_MEDIUM (message), "Resent-Bcc"))
		camel_medium_remove_header (CAMEL_MEDIUM (message), "Resent-Bcc");

	account = em_utils_guess_account_with_recipients (message, NULL);

	composer = e_msg_composer_new_redirect (
		shell, message, account ? account->name : NULL);

	return composer;
}

/**
 * em_utils_redirect_message:
 * @shell: an #EShell
 * @message: message to redirect
 *
 * Opens a composer to redirect @message (Note: only headers will be
 * editable). Adds Resent-From/Resent-To/etc headers.
 **/
void
em_utils_redirect_message (EShell *shell,
                           CamelMimeMessage *message)
{
	EMsgComposer *composer;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	composer = redirect_get_composer (shell, message);

	gtk_widget_show (GTK_WIDGET (composer));

	composer_set_no_change (composer);
}

static void
redirect_msg (CamelFolder *folder,
              const gchar *uid,
              CamelMimeMessage *message,
              gpointer user_data)
{
	EShell *shell = E_SHELL (user_data);

	if (message == NULL)
		return;

	em_utils_redirect_message (shell, message);

	g_object_unref (shell);
}

/**
 * em_utils_redirect_message_by_uid:
 * @shell: an #EShell
 * @folder: folder containing message to be redirected
 * @uid: uid of message to be redirected
 *
 * Opens a composer to redirect the message (Note: only headers will
 * be editable). Adds Resent-From/Resent-To/etc headers.
 **/
void
em_utils_redirect_message_by_uid (EShell *shell,
                                  CamelFolder *folder,
                                  const gchar *uid)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uid != NULL);

	mail_get_message (
		folder, uid, redirect_msg,
		g_object_ref (shell), mail_msg_unordered_push);
}

static void
emu_handle_receipt_message(CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer data, GError **error)
{
	if (msg)
		em_utils_handle_receipt(folder, uid, msg);

	/* we dont care really if we can't get the message */
	g_clear_error (error);
}

/* Message disposition notifications, rfc 2298 */
void
em_utils_handle_receipt (CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg)
{
	EAccount *account;
	const gchar *addr;
	CamelMessageInfo *info;

	info = camel_folder_get_message_info(folder, uid);
	if (info == NULL)
		return;

	if (camel_message_info_user_flag(info, "receipt-handled")) {
		camel_folder_free_message_info (folder, info);
		return;
	}

	if (msg == NULL) {
		mail_get_messagex(folder, uid, emu_handle_receipt_message, NULL, mail_msg_unordered_push);
		camel_folder_free_message_info (folder, info);
		return;
	}

	if ((addr = camel_medium_get_header((CamelMedium *)msg, "Disposition-Notification-To")) == NULL) {
		camel_folder_free_message_info (folder, info);
		return;
	}

	camel_message_info_set_user_flag(info, "receipt-handled", TRUE);
	camel_folder_free_message_info (folder, info);

	account = em_utils_guess_account_with_recipients (msg, folder);

	/* TODO: should probably decode/format the address, it could be in rfc2047 format */
	if (addr == NULL) {
		addr = "";
	} else {
		while (camel_mime_is_lwsp(*addr))
			addr++;
	}

	if (account && (account->receipt_policy == E_ACCOUNT_RECEIPT_ALWAYS || account->receipt_policy == E_ACCOUNT_RECEIPT_ASK)
	    && e_alert_run_dialog_for_args (e_shell_get_active_window (NULL), "mail:ask-receipt", addr, camel_mime_message_get_subject(msg), NULL) == GTK_RESPONSE_YES)
		em_utils_send_receipt(folder, msg);
}

static void
em_utils_receipt_done (CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info,
		       gint queued, const gchar *appended_uid, gpointer data)
{
	camel_message_info_free (info);
	mail_send ();
}

void
em_utils_send_receipt (CamelFolder *folder, CamelMimeMessage *message)
{
	/* See RFC #3798 for a description of message receipts */
	EAccount *account = em_utils_guess_account_with_recipients (message, folder);
	CamelMimeMessage *receipt = camel_mime_message_new ();
	CamelMultipart *body = camel_multipart_new ();
	CamelMimePart *part;
	CamelDataWrapper *receipt_text, *receipt_data;
	CamelContentType *type;
	CamelInternetAddress *addr;
	CamelStream *stream;
	CamelFolder *out_folder;
	CamelMessageInfo *info;
	const gchar *message_id = camel_medium_get_header (CAMEL_MEDIUM (message), "Message-ID");
	const gchar *message_date = camel_medium_get_header (CAMEL_MEDIUM (message), "Date");
	const gchar *message_subject = camel_mime_message_get_subject (message);
	const gchar *receipt_address = camel_medium_get_header (CAMEL_MEDIUM (message), "Disposition-Notification-To");
	gchar *fake_msgid;
	gchar *hostname;
	gchar *self_address, *receipt_subject;
	gchar *ua, *recipient;

	if (!receipt_address)
		return;

	/* the 'account' should be always set */
	g_return_if_fail (account != NULL);

	/* Collect information for the receipt */

	/* We use camel_header_msgid_generate () to get a canonical
	 * hostname, then skip the part leading to '@' */
	hostname = strchr ((fake_msgid = camel_header_msgid_generate ()), '@');
	hostname++;

	self_address = account->id->address;

	if (!message_id)
		message_id = "";
	if (!message_date)
		message_date ="";

	/* Create toplevel container */
	camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (body),
					  "multipart/report;"
					  "report-type=\"disposition-notification\"");
	camel_multipart_set_boundary (body, NULL);

	/* Create textual receipt */
	receipt_text = camel_data_wrapper_new ();
	type = camel_content_type_new ("text", "plain");
	camel_content_type_set_param (type, "format", "flowed");
	camel_content_type_set_param (type, "charset", "UTF-8");
	camel_data_wrapper_set_mime_type_field (receipt_text, type);
	camel_content_type_unref (type);
	stream = camel_stream_mem_new ();
	camel_stream_printf (stream,
	/* Translators: First %s is an email address, second %s is the subject of the email, third %s is the date */
			     _("Your message to %s about \"%s\" on %s has been read."),
			     self_address, message_subject, message_date);
	camel_data_wrapper_construct_from_stream (receipt_text, stream, NULL);
	g_object_unref (stream);

	part = camel_mime_part_new ();
	camel_medium_set_content (CAMEL_MEDIUM (part), receipt_text);
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE);
	g_object_unref (receipt_text);
	camel_multipart_add_part (body, part);
	g_object_unref (part);

	/* Create the machine-readable receipt */
	receipt_data = camel_data_wrapper_new ();
	part = camel_mime_part_new ();

	ua = g_strdup_printf ("%s; %s", hostname, "Evolution " VERSION SUB_VERSION " " VERSION_COMMENT);
	recipient = g_strdup_printf ("rfc822; %s", self_address);

	type = camel_content_type_new ("message", "disposition-notification");
	camel_data_wrapper_set_mime_type_field (receipt_data, type);
	camel_content_type_unref (type);

	stream = camel_stream_mem_new ();
	camel_stream_printf (stream,
			     "Reporting-UA: %s\n"
			     "Final-Recipient: %s\n"
			     "Original-Message-ID: %s\n"
			     "Disposition: manual-action/MDN-sent-manually; displayed\n",
			     ua, recipient, message_id);
	camel_data_wrapper_construct_from_stream (receipt_data, stream, NULL);
	g_object_unref (stream);

	g_free (ua);
	g_free (recipient);
	g_free (fake_msgid);

	camel_medium_set_content (CAMEL_MEDIUM (part), receipt_data);
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_7BIT);
	g_object_unref (receipt_data);
	camel_multipart_add_part (body, part);
	g_object_unref (part);

	/* Finish creating the message */
	camel_medium_set_content (CAMEL_MEDIUM (receipt), CAMEL_DATA_WRAPPER (body));
	g_object_unref (body);

	/* Translators: %s is the subject of the email message */
	receipt_subject = g_strdup_printf (_("Delivery Notification for: \"%s\""), message_subject);
	camel_mime_message_set_subject (receipt, receipt_subject);
	g_free (receipt_subject);

	addr = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (addr), self_address);
	camel_mime_message_set_from (receipt, addr);
	g_object_unref (addr);

	addr = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (addr), receipt_address);
	camel_mime_message_set_recipients (receipt, CAMEL_RECIPIENT_TYPE_TO, addr);
	g_object_unref (addr);

	camel_medium_set_header (CAMEL_MEDIUM (receipt), "Return-Path", "<>");
	camel_medium_set_header (CAMEL_MEDIUM (receipt), "X-Evolution-Account", account->uid);
	camel_medium_set_header (CAMEL_MEDIUM (receipt), "X-Evolution-Transport", account->transport->url);
	camel_medium_set_header (CAMEL_MEDIUM (receipt), "X-Evolution-Fcc",  account->sent_folder_uri);

	/* Send the receipt */
	info = camel_message_info_new (NULL);
	out_folder = e_mail_local_get_folder (E_MAIL_FOLDER_OUTBOX);
	camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	mail_append_mail (out_folder, receipt, info, em_utils_receipt_done, NULL);
}

/* Replying to messages... */

EDestination **
em_utils_camel_address_to_destination (CamelInternetAddress *iaddr)
{
	EDestination *dest, **destv;
	gint n, i, j;

	if (iaddr == NULL)
		return NULL;

	if ((n = camel_address_length ((CamelAddress *) iaddr)) == 0)
		return NULL;

	destv = g_malloc (sizeof (EDestination *) * (n + 1));
	for (i = 0, j = 0; i < n; i++) {
		const gchar *name, *addr;

		if (camel_internet_address_get (iaddr, i, &name, &addr)) {
			dest = e_destination_new ();
			e_destination_set_name (dest, name);
			e_destination_set_email (dest, addr);

			destv[j++] = dest;
		}
	}

	if (j == 0) {
		g_free (destv);
		return NULL;
	}

	destv[j] = NULL;

	return destv;
}

static EMsgComposer *
reply_get_composer (EShell *shell,
                    CamelMimeMessage *message,
                    EAccount *account,
                    CamelInternetAddress *to,
                    CamelInternetAddress *cc,
                    CamelFolder *folder,
                    CamelNNTPAddress *postto)
{
	const gchar *message_id, *references;
	EDestination **tov, **ccv;
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	gchar *subject;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);
	g_return_val_if_fail (to == NULL || CAMEL_IS_INTERNET_ADDRESS (to), NULL);
	g_return_val_if_fail (cc == NULL || CAMEL_IS_INTERNET_ADDRESS (cc), NULL);

	composer = e_msg_composer_new (shell);

	/* construct the tov/ccv */
	tov = em_utils_camel_address_to_destination (to);
	ccv = em_utils_camel_address_to_destination (cc);

	/* Set the subject of the new message. */
	if ((subject = (gchar *) camel_mime_message_get_subject (message))) {
		if (g_ascii_strncasecmp (subject, "Re: ", 4) != 0)
			subject = g_strdup_printf ("Re: %s", subject);
		else
			subject = g_strdup (subject);
	} else {
		subject = g_strdup ("");
	}

	table = e_msg_composer_get_header_table (composer);
	e_composer_header_table_set_account (table, account);
	e_composer_header_table_set_subject (table, subject);
	e_composer_header_table_set_destinations_to (table, tov);

	/* Add destinations instead of setting, so we don't remove
	 * automatic CC addresses that have already been added. */
	e_composer_header_table_add_destinations_cc (table, ccv);

	e_destination_freev (tov);
	e_destination_freev (ccv);
	g_free (subject);

	/* add post-to, if nessecary */
	if (postto && camel_address_length((CamelAddress *)postto)) {
		gchar *store_url = NULL;
		gchar *post;

		if (folder) {
			CamelStore *parent_store;

			parent_store = camel_folder_get_parent_store (folder);
			store_url = camel_url_to_string (
				CAMEL_SERVICE (parent_store)->url,
				CAMEL_URL_HIDE_ALL);
			if (store_url[strlen (store_url) - 1] == '/')
				store_url[strlen (store_url)-1] = '\0';
		}

		post = camel_address_encode((CamelAddress *)postto);
		e_composer_header_table_set_post_to_base (
			table, store_url ? store_url : "", post);
		g_free(post);
		g_free (store_url);
	}

	/* Add In-Reply-To and References. */
	message_id = camel_medium_get_header (CAMEL_MEDIUM (message), "Message-ID");
	references = camel_medium_get_header (CAMEL_MEDIUM (message), "References");
	if (message_id) {
		gchar *reply_refs;

		e_msg_composer_add_header (composer, "In-Reply-To", message_id);

		if (references)
			reply_refs = g_strdup_printf ("%s %s", references, message_id);
		else
			reply_refs = g_strdup (message_id);

		e_msg_composer_add_header (composer, "References", reply_refs);
		g_free (reply_refs);
	} else if (references) {
		e_msg_composer_add_header (composer, "References", references);
	}

	return composer;
}

static gboolean
get_reply_list (CamelMimeMessage *message, CamelInternetAddress *to)
{
	const gchar *header, *p;
	gchar *addr;

	/* Examples:
	 *
	 * List-Post: <mailto:list@host.com>
	 * List-Post: <mailto:moderator@host.com?subject=list%20posting>
	 * List-Post: NO (posting not allowed on this list)
	 */
	if (!(header = camel_medium_get_header ((CamelMedium *) message, "List-Post")))
		return FALSE;

	while (*header == ' ' || *header == '\t')
		header++;

	/* check for NO */
	if (!g_ascii_strncasecmp (header, "NO", 2))
		return FALSE;

	/* Search for the first mailto angle-bracket enclosed URL.
	 * (See rfc2369, Section 2, paragraph 3 for details) */
	if (!(header = camel_strstrcase (header, "<mailto:")))
		return FALSE;

	header += 8;

	p = header;
	while (*p && !strchr ("?>", *p))
		p++;

	addr = g_strndup (header, p - header);
	camel_internet_address_add(to, NULL, addr);
	g_free (addr);

	return TRUE;
}

gboolean
em_utils_is_munged_list_message(CamelMimeMessage *message)
{
	CamelInternetAddress *reply_to, *list;
	gboolean result = FALSE;

	reply_to = camel_mime_message_get_reply_to (message);
	if (reply_to) {
		list = camel_internet_address_new ();

		if (get_reply_list (message, list) &&
		    camel_address_length (CAMEL_ADDRESS(list)) == camel_address_length (CAMEL_ADDRESS(reply_to))) {
			gint i;
			const gchar *r_name, *r_addr;
			const gchar *l_name, *l_addr;

			for (i = 0; i < camel_address_length (CAMEL_ADDRESS(list)); i++) {
				if (!camel_internet_address_get (reply_to, i, &r_name, &r_addr))
					break;
				if (!camel_internet_address_get (list, i, &l_name, &l_addr))
					break;
				if (strcmp (l_addr, r_addr))
					break;
			}
			if (i == camel_address_length (CAMEL_ADDRESS(list)))
				result = TRUE;
		}
		g_object_unref (list);
	}
	return result;
}

static CamelInternetAddress *
get_reply_to (CamelMimeMessage *message)
{
	CamelInternetAddress *reply_to;

	reply_to = camel_mime_message_get_reply_to (message);
	if (reply_to) {
		GConfClient *gconf;
		gboolean ignore_list_reply_to;

		gconf = mail_config_get_gconf_client ();
		ignore_list_reply_to = gconf_client_get_bool (gconf,
					"/apps/evolution/mail/composer/ignore_list_reply_to", NULL);

		if (ignore_list_reply_to && em_utils_is_munged_list_message (message))
			reply_to = NULL;
	}
	if (!reply_to)
		reply_to = camel_mime_message_get_from (message);

	return reply_to;
}

static void
get_reply_sender (CamelMimeMessage *message, CamelInternetAddress *to, CamelNNTPAddress *postto)
{
	CamelInternetAddress *reply_to;
	const gchar *name, *addr, *posthdr;
	gint i;

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto
	    && ((posthdr = camel_medium_get_header((CamelMedium *)message, "Followup-To"))
		 || (posthdr = camel_medium_get_header((CamelMedium *)message, "Newsgroups")))) {
		camel_address_decode((CamelAddress *)postto, posthdr);
		return;
	}

	reply_to = get_reply_to (message);

	if (reply_to) {
		for (i = 0; camel_internet_address_get (reply_to, i, &name, &addr); i++)
			camel_internet_address_add (to, name, addr);
	}
}

void
em_utils_get_reply_sender (CamelMimeMessage *message, CamelInternetAddress *to, CamelNNTPAddress *postto)
{
	get_reply_sender (message, to, postto);
}

static void
get_reply_from (CamelMimeMessage *message, CamelInternetAddress *to, CamelNNTPAddress *postto)
{
	CamelInternetAddress *from;
	const gchar *name, *addr, *posthdr;
	gint i;

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto
	    && ((posthdr = camel_medium_get_header((CamelMedium *)message, "Followup-To"))
		 || (posthdr = camel_medium_get_header((CamelMedium *)message, "Newsgroups")))) {
		camel_address_decode((CamelAddress *)postto, posthdr);
		return;
	}

	from = camel_mime_message_get_from (message);

	if (from) {
		for (i = 0; camel_internet_address_get (from, i, &name, &addr); i++)
			camel_internet_address_add (to, name, addr);
	}
}

static void
concat_unique_addrs (CamelInternetAddress *dest, CamelInternetAddress *src, GHashTable *rcpt_hash)
{
	const gchar *name, *addr;
	gint i;

	for (i = 0; camel_internet_address_get (src, i, &name, &addr); i++) {
		if (!g_hash_table_lookup (rcpt_hash, addr)) {
			camel_internet_address_add (dest, name, addr);
			g_hash_table_insert (rcpt_hash, (gchar *) addr, GINT_TO_POINTER (1));
		}
	}
}

static void
get_reply_all (CamelMimeMessage *message, CamelInternetAddress *to, CamelInternetAddress *cc, CamelNNTPAddress *postto)
{
	CamelInternetAddress *reply_to, *to_addrs, *cc_addrs;
	const gchar *name, *addr, *posthdr;
	GHashTable *rcpt_hash;
	gint i;

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto) {
		if ((posthdr = camel_medium_get_header((CamelMedium *)message, "Followup-To")))
			camel_address_decode((CamelAddress *)postto, posthdr);
		if ((posthdr = camel_medium_get_header((CamelMedium *)message, "Newsgroups")))
			camel_address_decode((CamelAddress *)postto, posthdr);
	}

	rcpt_hash = em_utils_generate_account_hash ();

	reply_to = get_reply_to(message);
	to_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);

	if (reply_to) {
		for (i = 0; camel_internet_address_get (reply_to, i, &name, &addr); i++) {
			/* ignore references to the Reply-To address in the To and Cc lists */
			if (addr && !g_hash_table_lookup (rcpt_hash, addr)) {
				/* In the case that we are doing a Reply-To-All, we do not want
				   to include the user's email address because replying to oneself
				   is kinda silly. */

				camel_internet_address_add (to, name, addr);
				g_hash_table_insert (rcpt_hash, (gchar *) addr, GINT_TO_POINTER (1));
			}
		}
	}

	concat_unique_addrs (cc, to_addrs, rcpt_hash);
	concat_unique_addrs (cc, cc_addrs, rcpt_hash);

	/* promote the first Cc: address to To: if To: is empty */
	if (camel_address_length ((CamelAddress *) to) == 0 && camel_address_length ((CamelAddress *)cc) > 0) {
		camel_internet_address_get (cc, 0, &name, &addr);
		camel_internet_address_add (to, name, addr);
		camel_address_remove ((CamelAddress *)cc, 0);
	}

	/* if To: is still empty, may we removed duplicates (i.e. ourself), so add the original To if it was set */
	if (camel_address_length((CamelAddress *)to) == 0
	    && (camel_internet_address_get(to_addrs, 0, &name, &addr)
		|| camel_internet_address_get(cc_addrs, 0, &name, &addr))) {
		camel_internet_address_add(to, name, addr);
	}

	g_hash_table_destroy (rcpt_hash);
}

void
em_utils_get_reply_all (CamelMimeMessage *message, CamelInternetAddress *to, CamelInternetAddress *cc, CamelNNTPAddress *postto)
{
	get_reply_all (message, to, cc, postto);
}

enum {
	ATTRIB_UNKNOWN,
	ATTRIB_CUSTOM,
	ATTRIB_TIMEZONE,
	ATTRIB_STRFTIME,
	ATTRIB_TM_SEC,
	ATTRIB_TM_MIN,
	ATTRIB_TM_24HOUR,
	ATTRIB_TM_12HOUR,
	ATTRIB_TM_MDAY,
	ATTRIB_TM_MON,
	ATTRIB_TM_YEAR,
	ATTRIB_TM_2YEAR,
	ATTRIB_TM_WDAY, /* not actually used */
	ATTRIB_TM_YDAY
};

typedef void (* AttribFormatter) (GString *str, const gchar *attr, CamelMimeMessage *message);

static void
format_sender (GString *str, const gchar *attr, CamelMimeMessage *message)
{
	CamelInternetAddress *sender;
	const gchar *name, *addr = NULL;

	sender = camel_mime_message_get_from (message);
	if (sender != NULL && camel_address_length (CAMEL_ADDRESS (sender)) > 0) {
		camel_internet_address_get (sender, 0, &name, &addr);
	} else {
		name = _("an unknown sender");
	}

	if (name && !strcmp (attr, "{SenderName}")) {
		g_string_append (str, name);
	} else if (addr && !strcmp (attr, "{SenderEMail}")) {
		g_string_append (str, addr);
	} else if (name && *name) {
		g_string_append (str, name);
	} else if (addr) {
		g_string_append (str, addr);
	}
}

static struct {
	const gchar *name;
	gint type;
	struct {
		const gchar *format;         /* strftime or printf format */
		AttribFormatter formatter;  /* custom formatter */
	} v;
} attribvars[] = {
	{ "{Sender}",            ATTRIB_CUSTOM,    { NULL,    format_sender  } },
	{ "{SenderName}",        ATTRIB_CUSTOM,    { NULL,    format_sender  } },
	{ "{SenderEMail}",       ATTRIB_CUSTOM,    { NULL,    format_sender  } },
	{ "{AbbrevWeekdayName}", ATTRIB_STRFTIME,  { "%a",    NULL           } },
	{ "{WeekdayName}",       ATTRIB_STRFTIME,  { "%A",    NULL           } },
	{ "{AbbrevMonthName}",   ATTRIB_STRFTIME,  { "%b",    NULL           } },
	{ "{MonthName}",         ATTRIB_STRFTIME,  { "%B",    NULL           } },
	{ "{AmPmUpper}",         ATTRIB_STRFTIME,  { "%p",    NULL           } },
	{ "{AmPmLower}",         ATTRIB_STRFTIME,  { "%P",    NULL           } },
	{ "{Day}",               ATTRIB_TM_MDAY,   { "%02d",  NULL           } },  /* %d  01-31 */
	{ "{ Day}",              ATTRIB_TM_MDAY,   { "% 2d",  NULL           } },  /* %e   1-31 */
	{ "{24Hour}",            ATTRIB_TM_24HOUR, { "%02d",  NULL           } },  /* %H  00-23 */
	{ "{12Hour}",            ATTRIB_TM_12HOUR, { "%02d",  NULL           } },  /* %I  00-12 */
	{ "{DayOfYear}",         ATTRIB_TM_YDAY,   { "%d",    NULL           } },  /* %j  1-366 */
	{ "{Month}",             ATTRIB_TM_MON,    { "%02d",  NULL           } },  /* %m  01-12 */
	{ "{Minute}",            ATTRIB_TM_MIN,    { "%02d",  NULL           } },  /* %M  00-59 */
	{ "{Seconds}",           ATTRIB_TM_SEC,    { "%02d",  NULL           } },  /* %S  00-61 */
	{ "{2DigitYear}",        ATTRIB_TM_2YEAR,  { "%02d",  NULL           } },  /* %y */
	{ "{Year}",              ATTRIB_TM_YEAR,   { "%04d",  NULL           } },  /* %Y */
	{ "{TimeZone}",          ATTRIB_TIMEZONE,  { "%+05d", NULL           } }
};

/* Note to translators: this is the attribution string used when quoting messages.
 * each ${Variable} gets replaced with a value. To see a full list of available
 * variables, see em-composer-utils.c:1514 */
#define ATTRIBUTION _("On ${AbbrevWeekdayName}, ${Year}-${Month}-${Day} at ${24Hour}:${Minute} ${TimeZone}, ${Sender} wrote:")

static gchar *
attribution_format (const gchar *format, CamelMimeMessage *message)
{
	register const gchar *inptr;
	const gchar *start;
	gint tzone, len, i;
	gchar buf[64], *s;
	GString *str;
	struct tm tm;
	time_t date;
	gint type;

	str = g_string_new ("");

	date = camel_mime_message_get_date (message, &tzone);

	if (date == CAMEL_MESSAGE_DATE_CURRENT) {
		/* The message has no Date: header, look at Received: */
		date = camel_mime_message_get_date_received (message, &tzone);
	}
	if (date == CAMEL_MESSAGE_DATE_CURRENT) {
		/* That didn't work either, use current time */
		time (&date);
		tzone = 0;
	}

	/* Convert to UTC */
	date += (tzone / 100) * 60 * 60;
	date += (tzone % 100) * 60;

	gmtime_r (&date, &tm);

	inptr = format;
	while (*inptr != '\0') {
		start = inptr;
		while (*inptr && strncmp (inptr, "${", 2) != 0)
			inptr++;

		g_string_append_len (str, start, inptr - start);

		if (*inptr == '\0')
			break;

		start = ++inptr;
		while (*inptr && *inptr != '}')
			inptr++;

		if (*inptr != '}') {
			/* broken translation */
			g_string_append_len (str, "${", 2);
			inptr = start + 1;
			continue;
		}

		inptr++;
		len = inptr - start;
		type = ATTRIB_UNKNOWN;
		for (i = 0; i < G_N_ELEMENTS (attribvars); i++) {
			if (!strncmp (attribvars[i].name, start, len)) {
				type = attribvars[i].type;
				break;
			}
		}

		switch (type) {
		case ATTRIB_CUSTOM:
			attribvars[i].v.formatter (str, attribvars[i].name, message);
			break;
		case ATTRIB_TIMEZONE:
			g_string_append_printf (str, attribvars[i].v.format, tzone);
			break;
		case ATTRIB_STRFTIME:
			e_utf8_strftime (buf, sizeof (buf), attribvars[i].v.format, &tm);
			g_string_append (str, buf);
			break;
		case ATTRIB_TM_SEC:
			g_string_append_printf (str, attribvars[i].v.format, tm.tm_sec);
			break;
		case ATTRIB_TM_MIN:
			g_string_append_printf (str, attribvars[i].v.format, tm.tm_min);
			break;
		case ATTRIB_TM_24HOUR:
			g_string_append_printf (str, attribvars[i].v.format, tm.tm_hour);
			break;
		case ATTRIB_TM_12HOUR:
			g_string_append_printf (str, attribvars[i].v.format, (tm.tm_hour + 1) % 13);
			break;
		case ATTRIB_TM_MDAY:
			g_string_append_printf (str, attribvars[i].v.format, tm.tm_mday);
			break;
		case ATTRIB_TM_MON:
			g_string_append_printf (str, attribvars[i].v.format, tm.tm_mon + 1);
			break;
		case ATTRIB_TM_YEAR:
			g_string_append_printf (str, attribvars[i].v.format, tm.tm_year + 1900);
			break;
		case ATTRIB_TM_2YEAR:
			g_string_append_printf (str, attribvars[i].v.format, tm.tm_year % 100);
			break;
		case ATTRIB_TM_WDAY:
			/* not actually used */
			g_string_append_printf (str, attribvars[i].v.format, tm.tm_wday);
			break;
		case ATTRIB_TM_YDAY:
			g_string_append_printf (str, attribvars[i].v.format, tm.tm_yday + 1);
			break;
		default:
			/* mis-spelled variable? drop the format argument and continue */
			break;
		}
	}

	s = str->str;
	g_string_free (str, FALSE);

	return s;
}

static void
composer_set_body (EMsgComposer *composer, CamelMimeMessage *message, EMFormat *source)
{
	gchar *text, *credits;
	CamelMimePart *part;
	GConfClient *gconf;
	gssize len = 0;
	gboolean start_bottom;
	guint32 validity_found = 0;

	gconf = mail_config_get_gconf_client ();
	start_bottom = gconf_client_get_bool (gconf, "/apps/evolution/mail/composer/reply_start_bottom", NULL);

	switch (gconf_client_get_int (gconf, "/apps/evolution/mail/format/reply_style", NULL)) {
	case MAIL_CONFIG_REPLY_DO_NOT_QUOTE:
		/* do nothing */
		break;
	case MAIL_CONFIG_REPLY_ATTACH:
		/* attach the original message as an attachment */
		part = mail_tool_make_message_attachment (message);
		e_msg_composer_attach (composer, part);
		g_object_unref (part);
		break;
	case MAIL_CONFIG_REPLY_OUTLOOK:
		text = em_utils_message_to_html (message, _("-----Original Message-----"), EM_FORMAT_QUOTE_HEADERS, &len, source, start_bottom ? "<BR>" : NULL, &validity_found);
		e_msg_composer_set_body_text(composer, text, len);
		g_free (text);
		emu_update_composers_security (composer, validity_found);
		break;

	case MAIL_CONFIG_REPLY_QUOTED:
	default:
		/* do what any sane user would want when replying... */
		credits = attribution_format (ATTRIBUTION, message);
		text = em_utils_message_to_html (message, credits, EM_FORMAT_QUOTE_CITE, &len, source, start_bottom ? "<BR>" : NULL, &validity_found);
		g_free (credits);
		e_msg_composer_set_body_text(composer, text, len);
		g_free (text);
		emu_update_composers_security (composer, validity_found);
		break;
	}

	if (len > 0 && start_bottom) {
		GtkhtmlEditor *editor = GTKHTML_EDITOR (composer);

		/* If we are placing signature on top, then move cursor to the end,
		   otherwise try to find the signature place and place cursor just
		   before the signature. We added there an empty line already. */
		gtkhtml_editor_run_command (editor, "block-selection");
		gtkhtml_editor_run_command (editor, "cursor-bod");
		if (gconf_client_get_bool (gconf, "/apps/evolution/mail/composer/top_signature", NULL)
		    || !gtkhtml_editor_search_by_data (editor, 1, "ClueFlow", "signature", "1"))
			gtkhtml_editor_run_command (editor, "cursor-eod");
		else
			gtkhtml_editor_run_command (editor, "selection-move-left");
		gtkhtml_editor_run_command (editor, "unblock-selection");
	}
}

struct _reply_data {
	EShell *shell;
	EMFormat *source;
	gint mode;
};

gchar *
em_utils_construct_composer_text (CamelMimeMessage *message, EMFormat *source)
{
	gchar *text, *credits;
	gssize len = 0;
	gboolean start_bottom = 0;

	credits = attribution_format (ATTRIBUTION, message);
	text = em_utils_message_to_html (message, credits, EM_FORMAT_QUOTE_CITE, &len, source, start_bottom ? "<BR>" : NULL, NULL);

	g_free (credits);
	return text;
}

static void
reply_to_message (CamelFolder *folder,
                  const gchar *uid,
                  CamelMimeMessage *message,
                  gpointer user_data)
{
	struct _reply_data *rd = user_data;

	if (message != NULL) {
		/* get_message_free() will also unref the message, so we need
		   an extra ref for em_utils_reply_to_message() to drop. */
		g_object_ref(message);
		em_utils_reply_to_message (
			rd->shell, folder, uid, message, rd->mode, rd->source);
	}

	if (rd->shell != NULL)
		g_object_unref (rd->shell);

	if (rd->source != NULL)
		g_object_unref (rd->source);

	g_free (rd);
}

/**
 * em_utils_reply_to_message:
 * @shell: an #EShell
 * @folder: optional folder
 * @uid: optional uid
 * @message: message to reply to, optional
 * @mode: reply mode
 * @source: source to inherit view settings from
 *
 * Creates a new composer ready to reply to @message.
 *
 * If @message is NULL then @folder and @uid must be set to the
 * message to be replied to, it will be loaded asynchronously.
 *
 * If @message is non null, then it is used directly, @folder and @uid
 * may be supplied in order to update the message flags once it has
 * been replied to. Note that @message will be unreferenced on completion.
 **/
EMsgComposer *
em_utils_reply_to_message (EShell *shell,
                           CamelFolder *folder,
                           const gchar *uid,
                           CamelMimeMessage *message,
                           gint mode,
                           EMFormat *source)
{
	CamelInternetAddress *to, *cc;
	CamelNNTPAddress *postto = NULL;
	EMsgComposer *composer;
	EAccount *account;
	guint32 flags;
	struct emcs_t *emcs;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	if (folder && uid && message == NULL) {
		struct _reply_data *rd = g_malloc0(sizeof(*rd));

		rd->shell = g_object_ref (shell);
		rd->mode = mode;
		rd->source = source;
		if (rd->source)
			g_object_ref(rd->source);
		mail_get_message (
			folder, uid, reply_to_message,
			rd, mail_msg_unordered_push);

		return NULL;
	}

	g_return_val_if_fail(message != NULL, NULL);

	to = camel_internet_address_new();
	cc = camel_internet_address_new();

	account = em_utils_guess_account_with_recipients (message, folder);
	flags = CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_SEEN;

	switch (mode) {
	case REPLY_MODE_FROM:
		if (folder)
			postto = camel_nntp_address_new();

		get_reply_from (message, to, postto);
		break;
	case REPLY_MODE_SENDER:
		if (folder)
			postto = camel_nntp_address_new();

		get_reply_sender (message, to, postto);
		break;
	case REPLY_MODE_LIST:
		flags |= CAMEL_MESSAGE_ANSWERED_ALL;
		if (get_reply_list (message, to))
			break;
		/* falls through */
	case REPLY_MODE_ALL:
		flags |= CAMEL_MESSAGE_ANSWERED_ALL;
		if (folder)
			postto = camel_nntp_address_new();

		get_reply_all(message, to, cc, postto);
		break;
	}

	composer = reply_get_composer (
		shell, message, account, to, cc, folder, postto);
	e_msg_composer_add_message_attachments (composer, message, TRUE);

	if (postto)
		g_object_unref (postto);
	g_object_unref (to);
	g_object_unref (cc);

	composer_set_body (composer, message, source);

	g_object_unref(message);
	emcs = g_object_get_data (G_OBJECT (composer), "emcs");
	emcs_set_folder_info (emcs, folder, uid, flags, flags);

	composer_set_no_change (composer);

	gtk_widget_show (GTK_WIDGET (composer));

	return composer;
}

static void
post_header_clicked_cb (EComposerPostHeader *header,
                        EMsgComposer *composer)
{
	GtkTreeSelection *selection;
	GtkWidget *folder_tree;
	GtkWidget *dialog;
	GList *list;

	folder_tree = em_folder_tree_new ();
	emu_restore_folder_tree_state (EM_FOLDER_TREE (folder_tree));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	em_folder_tree_set_excluded (
		EM_FOLDER_TREE (folder_tree),
		EMFT_EXCLUDE_NOSELECT |
		EMFT_EXCLUDE_VIRTUAL |
		EMFT_EXCLUDE_VTRASH);

	dialog = em_folder_selector_new (
		GTK_WINDOW (composer),
		EM_FOLDER_TREE (folder_tree),
		EM_FOLDER_SELECTOR_CAN_CREATE,
		_("Posting destination"),
		_("Choose folders to post the message to."),
		NULL);

	list = e_composer_post_header_get_folders (header);
	em_folder_selector_set_selected_list (
		EM_FOLDER_SELECTOR (dialog), list);
	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
		/* Prevent the header's "custom" flag from being reset,
		 * which is what the default method will do next. */
		g_signal_stop_emission_by_name (header, "clicked");
		goto exit;
	}

	list = em_folder_selector_get_selected_uris (
		EM_FOLDER_SELECTOR (dialog));
	e_composer_post_header_set_folders (header, list);
	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);

exit:
	gtk_widget_destroy (dialog);
}

/**
 * em_configure_new_composer:
 * @composer: a newly created #EMsgComposer
 *
 * Integrates a newly created #EMsgComposer into the mail backend.  The
 * composer can't link directly to the mail backend without introducing
 * circular library dependencies, so this function finishes configuring
 * things the #EMsgComposer instance can't do itself.
 **/
void
em_configure_new_composer (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	EComposerHeaderType header_type;
	EComposerHeader *header;
	struct emcs_t *emcs;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	header_type = E_COMPOSER_HEADER_POST_TO;
	table = e_msg_composer_get_header_table (composer);
	header = e_composer_header_table_get_header (table, header_type);

	emcs = emcs_new ();

	g_object_set_data_full (
		G_OBJECT (composer), "emcs", emcs,
		(GDestroyNotify) emcs_unref);

	g_signal_connect (
		composer, "send",
		G_CALLBACK (em_utils_composer_send_cb), NULL);

	g_signal_connect (
		composer, "save-draft",
		G_CALLBACK (em_utils_composer_save_draft_cb), NULL);

	g_signal_connect (
		composer, "print",
		G_CALLBACK (em_utils_composer_print_cb), NULL);

	/* Handle "Post To:" button clicks, which displays a folder tree
	 * widget.  The composer doesn't know about folder tree widgets,
	 * so it can't handle this itself.
	 *
	 * Note: This is a G_SIGNAL_RUN_LAST signal, which allows us to
	 *       stop the signal emission if the user cancels or closes
	 *       the folder selector dialog.  See the handler function. */
	g_signal_connect (
		header, "clicked",
		G_CALLBACK (post_header_clicked_cb), composer);
}
