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
