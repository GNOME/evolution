/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <string.h>
#include <gtk/gtkdialog.h>

#include <gal/util/e-util.h>

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"
#include "mail-session.h"
#include "mail-send-recv.h"
#include "mail-component.h"

#include "widgets/misc/e-error.h"

#include "em-utils.h"
#include "em-composer-utils.h"
#include "composer/e-msg-composer.h"
#include "em-format-html.h"
#include "em-format-quote.h"

#include "e-util/e-account-list.h"

#include <camel/camel-string-utils.h>

static EAccount *guess_account (CamelMimeMessage *message, CamelFolder *folder);

struct emcs_t {
	unsigned int ref_count;
	
	CamelFolder *drafts_folder;
	char *drafts_uid;
	
	CamelFolder *folder;
	guint32 flags, set;
	char *uid;
};

static struct emcs_t *
emcs_new (void)
{
	struct emcs_t *emcs;
	
	emcs = g_new (struct emcs_t, 1);
	emcs->ref_count = 1;
	emcs->drafts_folder = NULL;
	emcs->drafts_uid = NULL;
	emcs->folder = NULL;
	emcs->flags = 0;
	emcs->set = 0;
	emcs->uid = NULL;
	
	return emcs;
}

static void
free_emcs (struct emcs_t *emcs)
{
	if (emcs->drafts_folder)
		camel_object_unref (emcs->drafts_folder);
	g_free (emcs->drafts_uid);
	
	if (emcs->folder)
		camel_object_unref (emcs->folder);
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

static void
composer_destroy_cb (gpointer user_data, GObject *deadbeef)
{
	emcs_unref (user_data);
}

static gboolean
ask_confirm_for_unwanted_html_mail (EMsgComposer *composer, EDestination **recipients)
{
	gboolean res;
	GString *str;
	int i;
	
	str = g_string_new("");
	for (i = 0; recipients[i] != NULL; ++i) {
		if (!e_destination_get_html_mail_pref (recipients[i])) {
			const char *name;
			
			name = e_destination_get_textrep (recipients[i], FALSE);
			
			g_string_append_printf (str, "     %s\n", name);
		}
	}

	res = em_utils_prompt_user((GtkWindow *)composer,"/apps/evolution/mail/prompts/unwanted_html",
				   "mail:ask-send-html", str->str, NULL);
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
			 int queued, const char *appended_uid, void *data)
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
			camel_object_unref (emcs->drafts_folder);
			emcs->drafts_folder = NULL;
			g_free (emcs->drafts_uid);
			emcs->drafts_uid = NULL;
		}
		
		if (emcs && emcs->folder) {
			/* set any replied flags etc */
			camel_folder_set_message_flags (emcs->folder, emcs->uid, emcs->flags, emcs->set);
			camel_object_unref (emcs->folder);
			emcs->folder = NULL;
			g_free (emcs->uid);
			emcs->uid = NULL;
		}
		
		gtk_widget_destroy (GTK_WIDGET (send->composer));
		
		if (send->send && camel_session_is_online (session)) {
			/* queue a message send */
			mail_send ();
		}
	} else {
		if (!emcs) {
			/* disconnect the previous signal handlers */
			g_signal_handlers_disconnect_matched (send->composer, G_SIGNAL_MATCH_FUNC, 0,
							      0, NULL, em_utils_composer_send_cb, NULL);
			g_signal_handlers_disconnect_matched (send->composer, G_SIGNAL_MATCH_FUNC, 0,
							      0, NULL, em_utils_composer_save_draft_cb, NULL);
			
			/* reconnect to the signals using a non-NULL emcs for the callback data */
			em_composer_utils_setup_default_callbacks (send->composer);
		}
		
		e_msg_composer_set_enable_autosave (send->composer, TRUE);
		gtk_widget_show (GTK_WIDGET (send->composer));
	}
	
	camel_message_info_free (info);
	
	if (send->emcs)
		emcs_unref (send->emcs);
	
	g_object_unref (send->composer);
	g_free (send);
}

static CamelMimeMessage *
composer_get_message (EMsgComposer *composer, gboolean post, gboolean save_html_object_data, gboolean *no_recipients)
{
	CamelMimeMessage *message = NULL;
	EDestination **recipients, **recipients_bcc;
	gboolean send_html, confirm_html;
	CamelInternetAddress *cia;
	int hidden = 0, shown = 0;
	int num = 0, num_bcc = 0;
	const char *subject;
	GConfClient *gconf;
	EAccount *account;
	int i;
	
	gconf = mail_config_get_gconf_client ();
	
	/* We should do all of the validity checks based on the composer, and not on
	   the created message, as extra interaction may occur when we get the message
	   (e.g. to get a passphrase to sign a message) */
	
	/* get the message recipients */
	recipients = e_msg_composer_get_recipients (composer);
	
	cia = camel_internet_address_new ();
	
	/* see which ones are visible/present, etc */
	if (recipients) {
		for (i = 0; recipients[i] != NULL; i++) {
			const char *addr = e_destination_get_address (recipients[i]);
			
			if (addr && addr[0]) {
				camel_address_decode ((CamelAddress *) cia, addr);
				if (camel_address_length ((CamelAddress *) cia) > 0) {
					camel_address_remove ((CamelAddress *) cia, -1);
					num++;
					if (e_destination_is_evolution_list (recipients[i])
					    && !e_destination_list_show_addresses (recipients[i])) {
						hidden++;
					} else {
						shown++;
					}
				}
			}
		}
	}
	
	recipients_bcc = e_msg_composer_get_bcc (composer);
	if (recipients_bcc) {
		for (i = 0; recipients_bcc[i] != NULL; i++) {
			const char *addr = e_destination_get_address (recipients_bcc[i]);
			
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
	
	camel_object_unref (cia);
	
	/* I'm sensing a lack of love, er, I mean recipients. */
	if (num == 0) {
		if (post) {
			if (no_recipients)
				*no_recipients = TRUE;
		} else {
			e_error_run((GtkWindow *)composer, "mail:send-no-recipients", NULL);
			goto finished;
		}
	}
	
	if (num > 0 && (num == num_bcc || shown == 0)) {
		/* this means that the only recipients are Bcc's */		
		if (!ask_confirm_for_only_bcc (composer, shown == 0))
			goto finished;
	}
	
	send_html = gconf_client_get_bool (gconf, "/apps/evolution/mail/composer/send_html", NULL);
	confirm_html = gconf_client_get_bool (gconf, "/apps/evolution/mail/prompts/unwanted_html", NULL);
	
	/* Only show this warning if our default is to send html.  If it isn't, we've
	   manually switched into html mode in the composer and (presumably) had a good
	   reason for doing this. */
	if (e_msg_composer_get_send_html (composer) && send_html && confirm_html) {
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
	subject = e_msg_composer_get_subject (composer);
	if (subject == NULL || subject[0] == '\0') {
		if (!ask_confirm_for_empty_subject (composer))
			goto finished;
	}
	
	/* actually get the message now, this will sign/encrypt etc */
	message = e_msg_composer_get_message (composer, save_html_object_data);
	if (message == NULL)
		goto finished;
	
	/* Add info about the sending account */
	account = e_msg_composer_get_preferred_account (composer);
	
	if (account) {
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Account", account->name);
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Transport", account->transport->url);
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-Evolution-Fcc", account->sent_folder_uri);
		if (account->id->organization && *account->id->organization) {
			char *org;
			
			org = camel_header_encode_string (account->id->organization);
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
got_post_folder (char *uri, CamelFolder *folder, void *data)
{
	CamelFolder **fp = data;
	
	*fp = folder;
	
	if (folder)
		camel_object_ref (folder);
}

void
em_utils_composer_send_cb (EMsgComposer *composer, gpointer user_data)
{
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	struct _send_data *send;
	gboolean no_recipients = FALSE;
	CamelFolder *mail_folder = NULL, *tmpfldr;
	GList *post_folders = NULL, *post_ptr;
	XEvolution *xev;
	GList *postlist;
	
	postlist = e_msg_composer_hdrs_get_post_to ((EMsgComposerHdrs *) composer->hdrs);
	while (postlist) {
		mail_msg_wait (mail_get_folder (postlist->data, 0, got_post_folder, &tmpfldr, mail_thread_new));
		if (tmpfldr)
			post_folders = g_list_append (post_folders, tmpfldr);
		postlist = g_list_next (postlist);
	}
	
	mail_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX);
	camel_object_ref (mail_folder);
	
	if (!post_folders && !mail_folder)
		return;
	
	if (!(message = composer_get_message (composer, post_folders != NULL, FALSE, &no_recipients)))
		return;
	
	if (no_recipients) {
		/* we're doing a post with no recipients */
		camel_object_unref (mail_folder);
		mail_folder = NULL;
	}
	
	if (mail_folder) {
		/* mail the message */
		info = camel_message_info_new ();
		info->flags = CAMEL_MESSAGE_SEEN;
		
		send = g_malloc (sizeof (*send));
		send->emcs = user_data;
		if (send->emcs)
			emcs_ref (send->emcs);
		send->send = TRUE;
		send->composer = composer;
		g_object_ref (composer);
		gtk_widget_hide (GTK_WIDGET (composer));
		
		e_msg_composer_set_enable_autosave (composer, FALSE);
		
		mail_append_mail (mail_folder, message, info, composer_send_queued_cb, send);
		camel_object_unref (mail_folder);
	}
	
	if (post_folders) {
		/* Remove the X-Evolution* headers if we are in Post-To mode */
		xev = mail_tool_remove_xevolution_headers (message);
		mail_tool_destroy_xevolution (xev);
		
		/* mail the message */
		info = camel_message_info_new ();
		info->flags = CAMEL_MESSAGE_SEEN;
		
		post_ptr = post_folders;
		while (post_ptr) {
			send = g_malloc (sizeof (*send));
			send->emcs = user_data;
			if (send->emcs)
				emcs_ref (send->emcs);
			send->send = FALSE;
			send->composer = composer;
			g_object_ref (composer);
			gtk_widget_hide (GTK_WIDGET (composer));
			
			e_msg_composer_set_enable_autosave (composer, FALSE);
			
			mail_append_mail ((CamelFolder *) post_ptr->data, message, info, composer_send_queued_cb, send);
			camel_object_unref ((CamelFolder *) post_ptr->data);
			
			post_ptr = g_list_next (post_ptr);
		}
	}
	
	camel_object_unref (message);
}

struct _save_draft_info {
	struct emcs_t *emcs;
	EMsgComposer *composer;
	int quit;
};

static void
save_draft_done (CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, int ok,
		 const char *appended_uid, void *user_data)
{
	struct _save_draft_info *sdi = user_data;
	struct emcs_t *emcs;
	CORBA_Environment ev;
	
	if (!ok)
		goto done;
	
	CORBA_exception_init (&ev);
	GNOME_GtkHTML_Editor_Engine_runCommand (sdi->composer->editor_engine, "saved", &ev);
	CORBA_exception_free (&ev);
	
	if ((emcs = sdi->emcs) == NULL) {
		emcs = emcs_new ();
		
		/* disconnect the previous signal handlers */
		g_signal_handlers_disconnect_by_func (sdi->composer, G_CALLBACK (em_utils_composer_send_cb), NULL);
		g_signal_handlers_disconnect_by_func (sdi->composer, G_CALLBACK (em_utils_composer_save_draft_cb), NULL);
		
		/* reconnect to the signals using a non-NULL emcs for the callback data */
		em_composer_utils_setup_default_callbacks (sdi->composer);
	}
	
	if (emcs->drafts_folder) {
		/* delete the original draft message */
		camel_folder_set_message_flags (emcs->drafts_folder, emcs->drafts_uid,
						CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN,
						CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN);
		camel_object_unref (emcs->drafts_folder);
		emcs->drafts_folder = NULL;
		g_free (emcs->drafts_uid);
		emcs->drafts_uid = NULL;
	}
	
	if (emcs->folder) {
		/* set the replied flags etc */
		camel_folder_set_message_flags (emcs->folder, emcs->uid, emcs->flags, emcs->set);
		camel_object_unref (emcs->folder);
		emcs->folder = NULL;
		g_free (emcs->uid);
		emcs->uid = NULL;
	}
	
	if (appended_uid) {
		camel_object_ref (folder);
		emcs->drafts_folder = folder;
		emcs->drafts_uid = g_strdup (appended_uid);
	}
	
	if (sdi->quit)
		gtk_widget_destroy (GTK_WIDGET (sdi->composer));
	
 done:
	g_object_unref (sdi->composer);
	if (sdi->emcs)
		emcs_unref (sdi->emcs);
	g_free (info);
	g_free (sdi);
}

static void
save_draft_folder (char *uri, CamelFolder *folder, gpointer data)
{
	CamelFolder **save = data;
	
	if (folder) {
		*save = folder;
		camel_object_ref (folder);
	}
}

void
em_utils_composer_save_draft_cb (EMsgComposer *composer, int quit, gpointer user_data)
{
	const char *default_drafts_folder_uri = mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS);
	CamelFolder *drafts_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_DRAFTS);
	struct _save_draft_info *sdi;
	CamelFolder *folder = NULL;
	CamelMimeMessage *msg;
	CamelMessageInfo *info;
	EAccount *account;
	
	account = e_msg_composer_get_preferred_account (composer);
	if (account && account->drafts_folder_uri &&
	    strcmp (account->drafts_folder_uri, default_drafts_folder_uri) != 0) {
		int id;
		
		id = mail_get_folder (account->drafts_folder_uri, 0, save_draft_folder, &folder, mail_thread_new);
		mail_msg_wait (id);
		
		if (!folder) {
			if (e_error_run((GtkWindow *)composer, "mail:ask-default-drafts", NULL) != GTK_RESPONSE_YES)
				return;
			
			folder = drafts_folder;
			camel_object_ref (drafts_folder);
		}
	} else {
		folder = drafts_folder;
		camel_object_ref (folder);
	}
	
	msg = e_msg_composer_get_message_draft (composer);
	
	info = g_new0 (CamelMessageInfo, 1);
	info->flags = CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_SEEN;
	
	sdi = g_malloc (sizeof (struct _save_draft_info));
	sdi->composer = composer;
	g_object_ref (composer);
	sdi->emcs = user_data;
	if (sdi->emcs)
		emcs_ref (sdi->emcs);
	sdi->quit = quit;
	
	mail_append_mail (folder, msg, info, save_draft_done, sdi);
	camel_object_unref (folder);
	camel_object_unref (msg);
}

void
em_composer_utils_setup_callbacks (EMsgComposer *composer, CamelFolder *folder, const char *uid,
				   guint32 flags, guint32 set, CamelFolder *drafts, const char *drafts_uid)
{
	struct emcs_t *emcs;
	
	emcs = emcs_new ();
	
	if (folder && uid) {
		camel_object_ref (folder);
		emcs->folder = folder;
		emcs->uid = g_strdup (uid);
		emcs->flags = flags;
		emcs->set = set;
	}
	
	if (drafts && drafts_uid) {
		camel_object_ref (drafts);
		emcs->drafts_folder = drafts;
		emcs->drafts_uid = g_strdup (drafts_uid);
	}
	
	g_signal_connect (composer, "send", G_CALLBACK (em_utils_composer_send_cb), emcs);
	g_signal_connect (composer, "save-draft", G_CALLBACK (em_utils_composer_save_draft_cb), emcs);
	
	g_object_weak_ref ((GObject *) composer, (GWeakNotify) composer_destroy_cb, emcs);
}

/* Composing messages... */

static EMsgComposer *
create_new_composer (const char *subject, const char *fromuri)
{
	EMsgComposer *composer;
	EAccount *account = NULL;

	composer = e_msg_composer_new ();
	
	if (fromuri)
		account = mail_config_get_account_by_source_url(fromuri);

	e_msg_composer_set_headers (composer, account?account->name:NULL, NULL, NULL, NULL, subject);

	em_composer_utils_setup_default_callbacks (composer);

	return composer;
}

/**
 * em_utils_compose_new_message:
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window.
 **/
void
em_utils_compose_new_message (const char *fromuri)
{
	GtkWidget *composer;

	composer = (GtkWidget *) create_new_composer ("", fromuri);

	e_msg_composer_unset_changed ((EMsgComposer *)composer);
	e_msg_composer_drop_editor_undo ((EMsgComposer *)composer);

	gtk_widget_show (composer);
}

/**
 * em_utils_compose_new_message_with_mailto:
 * @url: mailto url
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window. If @url is non-NULL, the composer fields will be filled in
 * according to the values in the mailto url.
 **/
void
em_utils_compose_new_message_with_mailto (const char *url, const char *fromuri)
{
	EMsgComposer *composer;
	EAccount *account = NULL;
	
	if (url != NULL)
		composer = e_msg_composer_new_from_url (url);
	else
		composer = e_msg_composer_new ();
	
	em_composer_utils_setup_default_callbacks (composer);

	if (fromuri
	    && (account = mail_config_get_account_by_source_url(fromuri)))
		e_msg_composer_set_headers (composer, account->name, NULL, NULL, NULL, "");

	e_msg_composer_unset_changed (composer);
	e_msg_composer_drop_editor_undo (composer);
	
	gtk_widget_show ((GtkWidget *) composer);
}

/**
 * em_utils_post_to_folder:
 * @folder: folder
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window. If @folder is non-NULL, the composer will default to posting
 * mail to the folder specified by @folder.
 **/
void
em_utils_post_to_folder (CamelFolder *folder)
{
	EMsgComposer *composer;
	EAccount *account;
	
	composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_POST);
	
	if (folder != NULL) {
		char *url = mail_tools_folder_to_url (folder);
		
		e_msg_composer_hdrs_set_post_to ((EMsgComposerHdrs *) ((EMsgComposer *) composer)->hdrs, url);
		g_free (url);
		
		url = camel_url_to_string (CAMEL_SERVICE (folder->parent_store)->url, CAMEL_URL_HIDE_ALL);
		account = mail_config_get_account_by_source_url (url);
		g_free (url);
		
		if (account)
			e_msg_composer_set_headers (composer, account->name, NULL, NULL, NULL, "");
	}
	
	em_composer_utils_setup_default_callbacks (composer);
	
	e_msg_composer_unset_changed (composer);
	e_msg_composer_drop_editor_undo (composer);
	
	gtk_widget_show ((GtkWidget *) composer);
}

/**
 * em_utils_post_to_url:
 * @url: mailto url
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window. If @url is non-NULL, the composer will default to posting
 * mail to the folder specified by @url.
 **/
void
em_utils_post_to_url (const char *url)
{
	EMsgComposer *composer;
	
	composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_POST);
	
	if (url != NULL)
		e_msg_composer_hdrs_set_post_to ((EMsgComposerHdrs *) ((EMsgComposer *) composer)->hdrs, url);
	
	em_composer_utils_setup_default_callbacks (composer);
	
	e_msg_composer_unset_changed (composer);
	e_msg_composer_drop_editor_undo (composer);
	
	gtk_widget_show ((GtkWidget *) composer);
}

/* Editing messages... */

static void
edit_message (CamelMimeMessage *message, CamelFolder *drafts, const char *uid)
{
	EMsgComposer *composer;

	composer = e_msg_composer_new_with_message (message);
	em_composer_utils_setup_callbacks (composer, NULL, NULL, 0, 0, drafts, uid);
	e_msg_composer_unset_changed (composer);
	e_msg_composer_drop_editor_undo (composer);
	
	gtk_widget_show (GTK_WIDGET (composer));
}

/**
 * em_utils_edit_message:
 * @message: message to edit
 *
 * Opens a composer filled in with the headers/mime-parts/etc of
 * @message.
 **/
void
em_utils_edit_message (CamelMimeMessage *message)
{
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	
	edit_message (message, NULL, NULL);
}

static void
edit_messages (CamelFolder *folder, GPtrArray *uids, GPtrArray *msgs, void *user_data)
{
	int i;
	
	if (msgs == NULL)
		return;
	
	for (i = 0; i < msgs->len; i++) {
		camel_medium_remove_header (CAMEL_MEDIUM (msgs->pdata[i]), "X-Mailer");
		
		edit_message (msgs->pdata[i], folder, uids->pdata[i]);
	}
}

/**
 * em_utils_edit_messages:
 * @folder: folder containing messages to edit
 * @uids: uids of messages to edit
 *
 * Opens a composer for each message to be edited.
 **/
void
em_utils_edit_messages (CamelFolder *folder, GPtrArray *uids)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	mail_get_messages (folder, uids, edit_messages, NULL);
}

/* Forwarding messages... */
static void
forward_attached (CamelFolder *folder, GPtrArray *messages, CamelMimePart *part, char *subject, const char *fromuri)
{
	EMsgComposer *composer;
	
	composer = create_new_composer (subject, fromuri);
	e_msg_composer_attach (composer, part);
	
	e_msg_composer_unset_changed (composer);
	e_msg_composer_drop_editor_undo (composer);
	
	gtk_widget_show (GTK_WIDGET (composer));
}

static void
forward_attached_cb (CamelFolder *folder, GPtrArray *messages, CamelMimePart *part, char *subject, void *user_data)
{
	if (part)
		forward_attached(folder, messages, part, subject, (char *)user_data);
	g_free(user_data);
}

/**
 * em_utils_forward_attached:
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 * @fromuri: from folder uri
 *
 * If there is more than a single message in @uids, a multipart/digest
 * will be constructed and attached to a new composer window preset
 * with the appropriate header defaults for forwarding the first
 * message in the list. If only one message is to be forwarded, it is
 * forwarded as a simple message/rfc822 attachment.
 **/
void
em_utils_forward_attached (CamelFolder *folder, GPtrArray *uids, const char *fromuri)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	mail_build_attachment (folder, uids, forward_attached_cb, g_strdup(fromuri));
}

static void
forward_non_attached (GPtrArray *messages, int style, const char *fromuri)
{
	CamelMimeMessage *message;
	CamelDataWrapper *wrapper;
	EMsgComposer *composer;
	char *subject, *text;
	int i;
	guint32 flags;

	if (messages->len == 0)
		return;

	flags = EM_FORMAT_QUOTE_HEADERS;
	if (style == MAIL_CONFIG_FORWARD_QUOTED)
		flags |= EM_FORMAT_QUOTE_CITE;

	for (i = 0; i < messages->len; i++) {
		message = messages->pdata[i];
		subject = mail_tool_generate_forward_subject (message);
		
		text = em_utils_message_to_html (message, _("-------- Forwarded Message --------"), flags);
		
		if (text) {
			composer = create_new_composer (subject, fromuri);

			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (message));
			if (CAMEL_IS_MULTIPART (wrapper))
				e_msg_composer_add_message_attachments (composer, message, FALSE);

			e_msg_composer_set_body_text (composer, text);
						
			e_msg_composer_unset_changed (composer);
			e_msg_composer_drop_editor_undo (composer);
			
			gtk_widget_show (GTK_WIDGET (composer));
			
			g_free (text);
		}
		
		g_free (subject);
	}
}

static void
forward_inline (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, void *user_data)
{
	forward_non_attached (messages, MAIL_CONFIG_FORWARD_INLINE, (char *)user_data);
	g_free(user_data);
}

/**
 * em_utils_forward_inline:
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 * @fromuri: from folder/account uri
 *
 * Forwards each message in the 'inline' form, each in its own composer window.
 **/
void
em_utils_forward_inline (CamelFolder *folder, GPtrArray *uids, const char *fromuri)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	mail_get_messages (folder, uids, forward_inline, g_strdup(fromuri));
}

static void
forward_quoted (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, void *user_data)
{
	forward_non_attached (messages, MAIL_CONFIG_FORWARD_QUOTED, (char *)user_data);
	g_free(user_data);
}

/**
 * em_utils_forward_quoted:
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 * @fromuri: from folder uri
 *
 * Forwards each message in the 'quoted' form (each line starting with
 * a "> "), each in its own composer window.
 **/
void
em_utils_forward_quoted (CamelFolder *folder, GPtrArray *uids, const char *fromuri)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	mail_get_messages (folder, uids, forward_quoted, g_strdup(fromuri));
}

/**
 * em_utils_forward_message:
 * @parent: parent window
 * @message: message to be forwarded
 * @fromuri: from folder uri
 *
 * Forwards a message in the user's configured default style.
 **/
void
em_utils_forward_message (CamelMimeMessage *message, const char *fromuri)
{
	GPtrArray *messages;
	CamelMimePart *part;
	GConfClient *gconf;
	char *subject;
	int mode;
	
	messages = g_ptr_array_new ();
	g_ptr_array_add (messages, message);
	
	gconf = mail_config_get_gconf_client ();
	mode = gconf_client_get_int (gconf, "/apps/evolution/mail/format/forward_style", NULL);
	
	switch (mode) {
	case MAIL_CONFIG_FORWARD_ATTACHED:
	default:
		part = mail_tool_make_message_attachment (message);
		
		subject = mail_tool_generate_forward_subject (message);
		
		forward_attached (NULL, messages, part, subject, fromuri);
		camel_object_unref (part);
		g_free (subject);
		break;
	case MAIL_CONFIG_FORWARD_INLINE:
		forward_non_attached (messages, MAIL_CONFIG_FORWARD_INLINE, fromuri);
		break;
	case MAIL_CONFIG_FORWARD_QUOTED:
		forward_non_attached (messages, MAIL_CONFIG_FORWARD_QUOTED, fromuri);
		break;
	}
	
	g_ptr_array_free (messages, TRUE);
}

/**
 * em_utils_forward_messages:
 * @folder: folder containing messages to forward
 * @uids: uids of messages to forward
 *
 * Forwards a group of messages in the user's configured default
 * style.
 **/
void
em_utils_forward_messages (CamelFolder *folder, GPtrArray *uids, const char *fromuri)
{
	GConfClient *gconf;
	int mode;
	
	gconf = mail_config_get_gconf_client ();
	mode = gconf_client_get_int (gconf, "/apps/evolution/mail/format/forward_style", NULL);
	
	switch (mode) {
	case MAIL_CONFIG_FORWARD_ATTACHED:
	default:
		em_utils_forward_attached (folder, uids, fromuri);
		break;
	case MAIL_CONFIG_FORWARD_INLINE:
		em_utils_forward_inline (folder, uids, fromuri);
		break;
	case MAIL_CONFIG_FORWARD_QUOTED:
		em_utils_forward_quoted (folder, uids, fromuri);
		break;
	}
}

/* Redirecting messages... */

static EMsgComposer *
redirect_get_composer (CamelMimeMessage *message)
{
	EMsgComposer *composer;
	EAccount *account;
	
	/* QMail will refuse to send a message if it finds one of
	   it's Delivered-To headers in the message, so remove all
	   Delivered-To headers. Fixes bug #23635. */
	while (camel_medium_get_header (CAMEL_MEDIUM (message), "Delivered-To"))
		camel_medium_remove_header (CAMEL_MEDIUM (message), "Delivered-To");
	
	account = guess_account (message, NULL);
	
	composer = e_msg_composer_new_redirect (message, account ? account->name : NULL);
	
	em_composer_utils_setup_default_callbacks (composer);
	
	return composer;
}

/**
 * em_utils_redirect_message:
 * @message: message to redirect
 *
 * Opens a composer to redirect @message (Note: only headers will be
 * editable). Adds Resent-From/Resent-To/etc headers.
 **/
void
em_utils_redirect_message (CamelMimeMessage *message)
{
	EMsgComposer *composer;
	CamelDataWrapper *wrapper;
	
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	
	composer = redirect_get_composer (message);
	
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (message));
	
	gtk_widget_show (GTK_WIDGET (composer));
	e_msg_composer_unset_changed (composer);
	e_msg_composer_drop_editor_undo (composer);
}

static void
redirect_msg (CamelFolder *folder, const char *uid, CamelMimeMessage *message, void *user_data)
{
	if (message == NULL)
		return;
	
	em_utils_redirect_message (message);
}

/**
 * em_utils_redirect_message_by_uid:
 * @folder: folder containing message to be redirected
 * @uid: uid of message to be redirected
 *
 * Opens a composer to redirect the message (Note: only headers will
 * be editable). Adds Resent-From/Resent-To/etc headers.
 **/
void
em_utils_redirect_message_by_uid (CamelFolder *folder, const char *uid)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uid != NULL);
	
	mail_get_message (folder, uid, redirect_msg, NULL, mail_thread_new);
}

/* Replying to messages... */

static GHashTable *
generate_account_hash (void)
{
	GHashTable *account_hash;
	EAccount *account, *def;
	EAccountList *accounts;
	EIterator *iter;
	
	accounts = mail_config_get_accounts ();
	account_hash = g_hash_table_new (camel_strcase_hash, camel_strcase_equal);
	
	/* add the default account to the hash first */
	if ((def = mail_config_get_default_account ())) {
		if (def->id->address)
			g_hash_table_insert (account_hash, (char *) def->id->address, (void *) def);
	}
	
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		
		if (account->id->address) {
			EAccount *acnt;
			
			/* Accounts with identical email addresses that are enabled
			 * take precedence over the accounts that aren't. If all
			 * accounts with matching email addresses are disabled, then
			 * the first one in the list takes precedence. The default
			 * account always takes precedence no matter what.
			 */
			acnt = g_hash_table_lookup (account_hash, account->id->address);
			if (acnt && acnt != def && !acnt->enabled && account->enabled) {
				g_hash_table_remove (account_hash, acnt->id->address);
				acnt = NULL;
			}
			
			if (!acnt)
				g_hash_table_insert (account_hash, (char *) account->id->address, (void *) account);
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	return account_hash;
}

static EDestination **
em_utils_camel_address_to_destination (CamelInternetAddress *iaddr)
{
	EDestination *dest, **destv;
	int n, i, j;
	
	if (iaddr == NULL)
		return NULL;
	
	if ((n = camel_address_length ((CamelAddress *) iaddr)) == 0)
		return NULL;
	
	destv = g_malloc (sizeof (EDestination *) * (n + 1));
	for (i = 0, j = 0; i < n; i++) {
		const char *name, *addr;
		
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
reply_get_composer (CamelMimeMessage *message, EAccount *account,
		    CamelInternetAddress *to, CamelInternetAddress *cc,
		    CamelFolder *folder, const char *postto)
{
	const char *message_id, *references;
	EDestination **tov, **ccv;
	EMsgComposer *composer;
	char *subject;
	
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);
	g_return_val_if_fail (to == NULL || CAMEL_IS_INTERNET_ADDRESS (to), NULL);
	g_return_val_if_fail (cc == NULL || CAMEL_IS_INTERNET_ADDRESS (cc), NULL);

	/* construct the tov/ccv */
	tov = em_utils_camel_address_to_destination (to);
	ccv = em_utils_camel_address_to_destination (cc);

	if (tov || ccv) {
		if (postto)
			composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_MAIL_POST);
		else
			composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_MAIL);
	} else
		composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_POST);

	/* Set the subject of the new message. */
	if ((subject = (char *) camel_mime_message_get_subject (message))) {
		if (strncasecmp (subject, "Re: ", 4) != 0)
			subject = g_strdup_printf ("Re: %s", subject);
		else
			subject = g_strdup (subject);
	} else {
		subject = g_strdup ("");
	}

	e_msg_composer_set_headers (composer, account ? account->name : NULL, tov, ccv, NULL, subject);
	
	g_free (subject);
	
	/* add post-to, if nessecary */
	if (postto) {
		char *store_url = NULL;
		
		if (folder) {
			store_url = camel_url_to_string (CAMEL_SERVICE (folder->parent_store)->url, CAMEL_URL_HIDE_ALL);
			if (store_url[strlen (store_url) - 1] == '/')
				store_url[strlen (store_url)-1] = '\0';
		}
		
		e_msg_composer_hdrs_set_post_to_base (E_MSG_COMPOSER_HDRS (composer->hdrs), store_url ? store_url : "", postto);
		g_free (store_url);
	}
	
	/* Add In-Reply-To and References. */
	message_id = camel_medium_get_header (CAMEL_MEDIUM (message), "Message-Id");
	references = camel_medium_get_header (CAMEL_MEDIUM (message), "References");
	if (message_id) {
		char *reply_refs;
		
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
	
	e_msg_composer_drop_editor_undo (composer);
	
	return composer;
}

static EAccount *
guess_account_folder(CamelFolder *folder)
{
	EAccount *account;
	char *tmp;

	tmp = camel_url_to_string(CAMEL_SERVICE(folder->parent_store)->url, CAMEL_URL_HIDE_ALL);
	account = mail_config_get_account_by_source_url(tmp);
	g_free(tmp);

	return account;
}

static EAccount *
guess_account (CamelMimeMessage *message, CamelFolder *folder)
{
	GHashTable *account_hash = NULL;
	EAccount *account = NULL;
	const char *tmp;
	int i, j;
	char *types[2] = { CAMEL_RECIPIENT_TYPE_TO, CAMEL_RECIPIENT_TYPE_CC };

	/* check for newsgroup header */
	if (folder
	    && camel_medium_get_header((CamelMedium *)message, "Newsgroups")
	    && (account = guess_account_folder(folder)))
		return account;

	/* then recipient (to/cc) in account table */
	account_hash = generate_account_hash ();
	for (j=0;account == NULL && j<2;j++) {
		const CamelInternetAddress *to;
		
		to = camel_mime_message_get_recipients(message, types[j]);
		if (to) {
			for (i = 0; camel_internet_address_get(to, i, NULL, &tmp); i++) {
				account = g_hash_table_lookup(account_hash, tmp);
				if (account)
					break;
			}
		}
	}
	g_hash_table_destroy(account_hash);

	/* then message source */
	if (account == NULL
	    && (tmp = camel_mime_message_get_source(message)))
		account = mail_config_get_account_by_source_url(tmp);

	/* and finally, source folder */
	if (account == NULL
	    && folder)
		account = guess_account_folder(folder);

	return account;
}

static void
get_reply_sender (CamelMimeMessage *message, CamelInternetAddress **to, const char **postto)
{
	const CamelInternetAddress *reply_to;
	const char *name, *addr, *posthdr;
	int i;
	
	/* check whether there is a 'Newsgroups: ' header in there */
	posthdr = camel_medium_get_header (CAMEL_MEDIUM (message), "Newsgroups");
	if (posthdr && postto) {
		*postto = posthdr;
		while (**postto == ' ')
			(*postto)++;
		return;
	}
	
	reply_to = camel_mime_message_get_reply_to (message);
	if (!reply_to)
		reply_to = camel_mime_message_get_from (message);
	
	if (reply_to) {
		*to = camel_internet_address_new ();
		
		for (i = 0; camel_internet_address_get (reply_to, i, &name, &addr); i++)
			camel_internet_address_add (*to, name, addr);
	}
}

static gboolean
get_reply_list (CamelMimeMessage *message, CamelInternetAddress **to)
{
	const char *header, *p;
	char *addr;
	
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
	if (!strncasecmp (header, "NO", 2))
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
	
	*to = camel_internet_address_new ();
	camel_internet_address_add (*to, NULL, addr);
	
	g_free (addr);
	
	return TRUE;
}

static void
concat_unique_addrs (CamelInternetAddress *dest, const CamelInternetAddress *src, GHashTable *rcpt_hash)
{
	const char *name, *addr;
	int i;
	
	for (i = 0; camel_internet_address_get (src, i, &name, &addr); i++) {
		if (!g_hash_table_lookup (rcpt_hash, addr)) {
			camel_internet_address_add (dest, name, addr);
			g_hash_table_insert (rcpt_hash, (char *) addr, GINT_TO_POINTER (1));
		}
	}
}

static void
get_reply_all (CamelMimeMessage *message, CamelInternetAddress **to, CamelInternetAddress **cc, const char **postto)
{
	const CamelInternetAddress *reply_to, *to_addrs, *cc_addrs;
	const char *name, *addr, *posthdr;
	GHashTable *rcpt_hash;
	int i;
	
	/* check whether there is a 'Newsgroups: ' header in there */
	posthdr = camel_medium_get_header (CAMEL_MEDIUM(message), "Newsgroups");
	if (posthdr && postto) {
		*postto = posthdr;
		while (**postto == ' ')
			(*postto)++;
	}
	
	rcpt_hash = generate_account_hash ();
	
	reply_to = camel_mime_message_get_reply_to (message);
	if (!reply_to)
		reply_to = camel_mime_message_get_from (message);
	
	to_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	
	*to = camel_internet_address_new ();
	*cc = camel_internet_address_new ();
	
	if (reply_to) {
		for (i = 0; camel_internet_address_get (reply_to, i, &name, &addr); i++) {
			/* ignore references to the Reply-To address in the To and Cc lists */
			if (addr && !g_hash_table_lookup (rcpt_hash, addr)) {
				/* In the case that we are doing a Reply-To-All, we do not want
				   to include the user's email address because replying to oneself
				   is kinda silly. */
				
				camel_internet_address_add (*to, name, addr);
				g_hash_table_insert (rcpt_hash, (char *) addr, GINT_TO_POINTER (1));
			}
		}
	}
	
	concat_unique_addrs (*cc, to_addrs, rcpt_hash);
	concat_unique_addrs (*cc, cc_addrs, rcpt_hash);
	
	/* promote the first Cc: address to To: if To: is empty */
	if (camel_address_length ((CamelAddress *) *to) == 0 && camel_address_length ((CamelAddress *) *cc) > 0) {
		camel_internet_address_get (*cc, 0, &name, &addr);
		camel_internet_address_add (*to, name, addr);
		camel_address_remove ((CamelAddress *) *cc, 0);
	}
	
	g_hash_table_destroy (rcpt_hash);
}

static void
composer_set_body (EMsgComposer *composer, CamelMimeMessage *message)
{
	const CamelInternetAddress *sender;
	char *text, *credits, format[256];
	const char *name, *addr;
	CamelMimePart *part;
	GConfClient *gconf;
	time_t date;
	int date_offset;
	
	gconf = mail_config_get_gconf_client ();
	
	switch (gconf_client_get_int (gconf, "/apps/evolution/mail/format/reply_style", NULL)) {
	case MAIL_CONFIG_REPLY_DO_NOT_QUOTE:
		/* do nothing */
		break;
	case MAIL_CONFIG_REPLY_ATTACH:
		/* attach the original message as an attachment */
		part = mail_tool_make_message_attachment (message);
		e_msg_composer_attach (composer, part);
		camel_object_unref (part);
		break;
	case MAIL_CONFIG_REPLY_QUOTED:
	default:
		/* do what any sane user would want when replying... */
		sender = camel_mime_message_get_from (message);
		if (sender != NULL && camel_address_length (CAMEL_ADDRESS (sender)) > 0) {
			camel_internet_address_get (sender, 0, &name, &addr);
		} else {
			name = _("an unknown sender");
		}

		date = camel_mime_message_get_date(message, &date_offset);
		/* Convert to UTC */
		date += (date_offset / 100) * 60 * 60;
		date += (date_offset % 100) * 60;

		/* translators: attribution string used when quoting messages,
		   it must contain a single single %%+05d followed by a single '%%s' */
		e_utf8_strftime(format, sizeof(format), _("On %a, %Y-%m-%d at %H:%M %%+05d, %%s wrote:"), gmtime(&date));
		credits = g_strdup_printf(format, date_offset, name && *name ? name : addr);
		text = em_utils_message_to_html(message, credits, EM_FORMAT_QUOTE_CITE);
		g_free (credits);
		e_msg_composer_set_body_text(composer, text);
		g_free (text);
		break;
	}
	
	e_msg_composer_drop_editor_undo (composer);
}

/**
 * em_utils_reply_to_message:
 * @message: message to reply to
 * @mode: reply mode
 *
 * Creates a new composer ready to reply to @message.
 **/
void
em_utils_reply_to_message (CamelMimeMessage *message, int mode)
{
	CamelInternetAddress *to = NULL, *cc = NULL;
	EMsgComposer *composer;
	EAccount *account;
	
	account = guess_account (message, NULL);
	
	switch (mode) {
	case REPLY_MODE_SENDER:
		get_reply_sender (message, &to, NULL);
		break;
	case REPLY_MODE_LIST:
		if (get_reply_list (message, &to))
			break;
	case REPLY_MODE_ALL:
		get_reply_all (message, &to, &cc, NULL);
		break;
	}
	
	composer = reply_get_composer (message, account, to, cc, NULL, NULL);
	e_msg_composer_add_message_attachments (composer, message, TRUE);
	
	if (to != NULL)
		camel_object_unref (to);
	
	if (cc != NULL)
		camel_object_unref (cc);
	
	composer_set_body (composer, message);
	
	em_composer_utils_setup_default_callbacks (composer);
	
	gtk_widget_show (GTK_WIDGET (composer));
	e_msg_composer_unset_changed (composer);
}

static void
reply_to_message (CamelFolder *folder, const char *uid, CamelMimeMessage *message, void *user_data)
{
	CamelInternetAddress *to = NULL, *cc = NULL;
	const char *postto = NULL;
	EMsgComposer *composer;
	EAccount *account;
	guint32 flags;
	int mode;
	
	if (message == NULL)
		return;
	
	mode = GPOINTER_TO_INT (user_data);

	account = guess_account (message, folder);
	flags = CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_SEEN;
	
	switch (mode) {
	case REPLY_MODE_SENDER:
		get_reply_sender (message, &to, &postto);
		break;
	case REPLY_MODE_LIST:
		flags |= CAMEL_MESSAGE_ANSWERED_ALL;
		if (get_reply_list (message, &to))
			break;
	case REPLY_MODE_ALL:
		flags |= CAMEL_MESSAGE_ANSWERED_ALL;
		get_reply_all (message, &to, &cc, &postto);
		break;
	}
	
	composer = reply_get_composer (message, account, to, cc, folder, postto);
	e_msg_composer_add_message_attachments (composer, message, TRUE);
	
	if (to != NULL)
		camel_object_unref (to);
	
	if (cc != NULL)
		camel_object_unref (cc);
	
	composer_set_body (composer, message);
	
	em_composer_utils_setup_callbacks (composer, folder, uid, flags, flags, NULL, NULL);
	
	gtk_widget_show (GTK_WIDGET (composer));
	e_msg_composer_unset_changed (composer);
}

/**
 * em_utils_reply_to_message_by_uid:
 * @folder: folder containing message to reply to
 * @uid: message uid
 * @mode: reply mode
 *
 * Creates a new composer ready to reply to the message referenced by
 * @folder and @uid.
 **/
void
em_utils_reply_to_message_by_uid (CamelFolder *folder, const char *uid, int mode)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uid != NULL);
	
	mail_get_message (folder, uid, reply_to_message, GINT_TO_POINTER (mode), mail_thread_new);
}

/* Posting replies... */

static void
post_reply_to_message (CamelFolder *folder, const char *uid, CamelMimeMessage *message, void *user_data)
{
	/* FIXME: would be nice if this shared more code with reply_get_composer() */
	const char *message_id, *references;
	CamelInternetAddress *to = NULL;
	EDestination **tov = NULL;
	EMsgComposer *composer;
	char *subject, *url;
	EAccount *account;
	guint32 flags;
	
	if (message == NULL)
		return;
	
	account = guess_account (message, folder);
	flags = CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_SEEN;
	
	get_reply_sender (message, &to, NULL);
	
	composer = e_msg_composer_new_with_type (E_MSG_COMPOSER_MAIL_POST);
	
	/* construct the tov/ccv */
	tov = em_utils_camel_address_to_destination (to);
	
	/* Set the subject of the new message. */
	if ((subject = (char *) camel_mime_message_get_subject (message))) {
		if (strncasecmp (subject, "Re: ", 4) != 0)
			subject = g_strdup_printf ("Re: %s", subject);
		else
			subject = g_strdup (subject);
	} else {
		subject = g_strdup ("");
	}
	
	e_msg_composer_set_headers (composer, account ? account->name : NULL, tov, NULL, NULL, subject);
	
	g_free (subject);
	
	url = mail_tools_folder_to_url (folder);
	e_msg_composer_hdrs_set_post_to ((EMsgComposerHdrs *) composer->hdrs, url);
	g_free (url);
	
	/* Add In-Reply-To and References. */
	message_id = camel_medium_get_header (CAMEL_MEDIUM (message), "Message-Id");
	references = camel_medium_get_header (CAMEL_MEDIUM (message), "References");
	if (message_id) {
		char *reply_refs;
		
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
	
	e_msg_composer_drop_editor_undo (composer);
	
	e_msg_composer_add_message_attachments (composer, message, TRUE);
	
	if (to != NULL)
		camel_object_unref (to);
	
	composer_set_body (composer, message);
	
	em_composer_utils_setup_callbacks (composer, folder, uid, flags, flags, NULL, NULL);
	
	gtk_widget_show (GTK_WIDGET (composer));	
	e_msg_composer_unset_changed (composer);
}

/**
 * em_utils_post_reply_to_message_by_uid:
 * @folder: folder containing message to reply to
 * @uid: message uid
 * @mode: reply mode
 *
 * Creates a new composer (post mode) ready to reply to the message
 * referenced by @folder and @uid.
 **/
void
em_utils_post_reply_to_message_by_uid (CamelFolder *folder, const char *uid)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uid != NULL);
	
	mail_get_message (folder, uid, post_reply_to_message, NULL, mail_thread_new);
}
