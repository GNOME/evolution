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
#include <e-util/e-util.h>
#include <glib/gi18n.h>

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"
#include "mail-session.h"
#include "mail-send-recv.h"
#include "mail-component.h"

#include "e-util/e-error.h"

#include "em-utils.h"
#include "em-composer-utils.h"
#include "composer/e-msg-composer.h"
#include "composer/e-composer-autosave.h"
#include "em-format-html.h"
#include "em-format-quote.h"
#include "em-event.h"

#include "libedataserver/e-account-list.h"

#include <camel/camel-folder.h>
#include <camel/camel-multipart.h>
#include <camel/camel-string-utils.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-nntp-address.h>
#include <camel/camel-vee-folder.h>

#ifdef G_OS_WIN32
/* Undef the similar macro from pthread.h, it doesn't check if
 * gmtime() returns NULL.
 */
#undef gmtime_r

/* The gmtime() in Microsoft's C library is MT-safe */
#define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
#endif

#define GCONF_KEY_TEMPLATE_PLACEHOLDERS "/apps/evolution/mail/template_placeholders"

static EAccount * guess_account (CamelMimeMessage *message, CamelFolder *folder);

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
			camel_object_unref (emcs->drafts_folder);
			emcs->drafts_folder = NULL;
			g_free (emcs->drafts_uid);
			emcs->drafts_uid = NULL;
		}

		if (emcs && emcs->folder) {
			/* set any replied flags etc */
			camel_folder_set_message_flags (emcs->folder, emcs->uid, emcs->flags, emcs->set);
			camel_folder_set_message_user_flag (emcs->folder, emcs->uid, "receipt-handled", TRUE);
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

	camel_object_unref (cia);

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
		e_error_run((GtkWindow *)composer, "mail:send-no-recipients", NULL);
		goto finished;
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
	message = e_msg_composer_get_message (composer, save_html_object_data);
	if (message == NULL)
		goto finished;

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

void
em_utils_composer_send_cb (EMsgComposer *composer, gpointer user_data)
{
	EComposerHeaderTable *table;
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	struct _send_data *send;
	CamelFolder *mail_folder;
	EAccount *account;

	table = e_msg_composer_get_header_table (composer);
	account = e_composer_header_table_get_account (table);
	if (!account || !account->enabled) {
		e_error_run((GtkWindow *)composer, "mail:send-no-account-enabled", NULL);
		return;
	}

	if (!(message = composer_get_message (composer, FALSE)))
		return;

	mail_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX);
	camel_object_ref (mail_folder);

	/* mail the message */
	e_msg_composer_set_mail_sent (composer, TRUE);
	info = camel_message_info_new(NULL);
	camel_message_info_set_flags(info, CAMEL_MESSAGE_SEEN, ~0);

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
	camel_object_unref (message);
}

struct _save_draft_info {
	struct emcs_t *emcs;
	EMsgComposer *composer;
	CamelMessageInfo *info;
};

static void
composer_set_no_change (EMsgComposer *composer, gboolean drop_undo, gboolean editor_changed)
{
	GtkhtmlEditor *editor;

	g_return_if_fail (composer != NULL);

	editor = GTKHTML_EDITOR (composer);

	if (drop_undo)
		gtkhtml_editor_drop_undo (editor);

	if (editor_changed)
		gtkhtml_editor_set_changed (editor, FALSE);

	e_composer_autosave_set_saved (composer, TRUE);
}

static void
save_draft_done (CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, gint ok,
		 const gchar *appended_uid, gpointer user_data)
{
	struct _save_draft_info *sdi = user_data;
	struct emcs_t *emcs;

	if (!ok)
		goto done;

	composer_set_no_change (sdi->composer, FALSE, FALSE);

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
		camel_object_ref (folder);
	}
}

void
em_utils_composer_save_draft_cb (EMsgComposer *composer, gpointer user_data)
{
	const gchar *default_drafts_folder_uri = mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS);
	CamelFolder *drafts_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_DRAFTS);
	EComposerHeaderTable *table;
	struct _save_draft_info *sdi;
	CamelFolder *folder = NULL;
	CamelMimeMessage *msg;
	CamelMessageInfo *info;
	EAccount *account;

	/* need to get stuff from the composer here, since it could
	 * get destroyed while we're in mail_msg_wait() a little lower
	 * down, waiting for the folder to open */

	g_object_ref(composer);
	msg = e_msg_composer_get_message_draft (composer);
	table = e_msg_composer_get_header_table (composer);
	account = e_composer_header_table_get_account (table);

	sdi = g_malloc(sizeof(struct _save_draft_info));
	sdi->composer = composer;
	sdi->emcs = user_data;
	if (sdi->emcs)
		emcs_ref(sdi->emcs);

	if (account && account->drafts_folder_uri &&
	    strcmp (account->drafts_folder_uri, default_drafts_folder_uri) != 0) {
		gint id;

		id = mail_get_folder (account->drafts_folder_uri, 0, save_draft_folder, &folder, mail_msg_unordered_push);
		mail_msg_wait (id);

		if (!folder || !account->enabled) {
			if (e_error_run((GtkWindow *)composer, "mail:ask-default-drafts", NULL) != GTK_RESPONSE_YES) {
				g_object_unref(composer);
				camel_object_unref(msg);
				if (sdi->emcs)
					emcs_unref(sdi->emcs);
				g_free(sdi);
				return;
			}

			folder = drafts_folder;
			camel_object_ref (drafts_folder);
		}
	} else {
		folder = drafts_folder;
		camel_object_ref (folder);
	}

	info = camel_message_info_new(NULL);
	camel_message_info_set_flags(info, CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_SEEN, ~0);

	mail_append_mail (folder, msg, info, save_draft_done, sdi);
	camel_object_unref (folder);
	camel_object_unref (msg);
}

void
em_composer_utils_setup_callbacks (EMsgComposer *composer,
                                   CamelFolder *folder,
                                   const gchar *uid,
				   guint32 flags,
                                   guint32 set,
                                   CamelFolder *drafts,
                                   const gchar *drafts_uid)
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
create_new_composer (const gchar *subject, const gchar *fromuri, gboolean use_default_callbacks, gboolean lite)
{
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	EAccount *account = NULL;

	if (lite)
		composer = e_msg_composer_lite_new ();
	else
		composer = e_msg_composer_new ();
	table = e_msg_composer_get_header_table (composer);

	if (fromuri != NULL) {
		GList *list;

		account = mail_config_get_account_by_source_url(fromuri);

		list = g_list_prepend (NULL, (gpointer) fromuri);
		e_composer_header_table_set_post_to_list (table, list);
		g_list_free (list);
	}

	e_composer_header_table_set_account (table, account);
	e_composer_header_table_set_subject (table, subject);

	if (use_default_callbacks)
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
em_utils_compose_new_message (const gchar *fromuri)
{
	GtkWidget *composer;

	composer = (GtkWidget *) create_new_composer ("", fromuri, TRUE, FALSE);
	if (composer == NULL)
		return;

	composer_set_no_change (E_MSG_COMPOSER (composer), TRUE, TRUE);

	gtk_widget_show (composer);
}

/**
 * em_utils_compose_lite_new_message:
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window.
 **/
struct _EMsgComposer *
em_utils_compose_lite_new_message (const gchar *fromuri)
{
	GtkWidget *composer;

	composer = (GtkWidget *) create_new_composer ("", fromuri, TRUE, TRUE);
	if (composer == NULL)
		return NULL;

	composer_set_no_change (E_MSG_COMPOSER (composer), TRUE, TRUE);

	return (struct _EMsgComposer *)composer;
}

/**
 * em_utils_compose_new_message_with_mailto:
 * @url: mailto url
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window. If @url is non-NULL, the composer fields will be filled in
 * according to the values in the mailto url.
 **/
struct _EMsgComposer *
em_utils_compose_new_message_with_mailto (const gchar *url, const gchar *fromuri)
{
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	EAccount *account = NULL;

	if (url != NULL)
		composer = e_msg_composer_new_from_url (url);
	else
		composer = e_msg_composer_new ();

	table = e_msg_composer_get_header_table (composer);
	em_composer_utils_setup_default_callbacks (composer);

	if (fromuri
	    && (account = mail_config_get_account_by_source_url(fromuri)))
		e_composer_header_table_set_account_name (table, account->name);

	composer_set_no_change (composer, TRUE, url == NULL);

	if (!e_msg_composer_get_lite()) {
		gtk_widget_show ((GtkWidget *) composer);
		gdk_window_raise (((GtkWidget *) composer)->window);
	}

	return composer;
}

/* Editing messages... */

static GtkWidget *
edit_message (CamelMimeMessage *message, CamelFolder *drafts, const gchar *uid)
{
	EMsgComposer *composer;

	/* Template specific code follows. */
	if (em_utils_folder_is_templates(drafts, NULL) == TRUE) {
		/* retrieve the message from the CamelFolder */
		CamelDataWrapper *content;
		CamelStream *mem;
		CamelContentType *type;
		CamelMimePart *mime_part = CAMEL_MIME_PART (message);
		CamelDataWrapper *mail_text;
		CamelMultipart *body = camel_multipart_new ();
		CamelStream *stream;
		CamelMimePart *part;
		gint count1 = 0, string_changed = 0;
		gint i;
		GConfClient *gconf;
		GSList *clue_list = NULL, *list;

		gchar *str;
		gint count = 2;

		content = camel_medium_get_content_object ((CamelMedium *) message);
		if (!content)
			return NULL;

		/*
		 * Get non-multipart content from multipart message.
		 */
		while (CAMEL_IS_MULTIPART (content) && count > 0)
		{
			mime_part = camel_multipart_get_part (CAMEL_MULTIPART (content), 0);
			content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
			count--;
		}

		if (!mime_part)
			return NULL;

		type = camel_mime_part_get_content_type (mime_part);
		if (!camel_content_type_is (type, "text", "plain"))
			return NULL;

		mem = camel_stream_mem_new ();
		camel_data_wrapper_decode_to_stream (content, mem);

		str = g_strndup ((const gchar *)((CamelStreamMem *) mem)->buffer->data, ((CamelStreamMem *) mem)->buffer->len);
		camel_object_unref (mem);

		gconf = gconf_client_get_default ();
		/* Get the list from gconf */
		clue_list = gconf_client_get_list ( gconf, GCONF_KEY_TEMPLATE_PLACEHOLDERS, GCONF_VALUE_STRING, NULL );
		g_object_unref (gconf);

		for (i = 0; i < strlen (str); i++) {
			const gchar *cur = str + i;
			if (!g_ascii_strncasecmp (cur, "$", 1)) {
				const gchar *end = cur + 1;
				gchar *out;
				gchar **temp_str;

				while (*end && (g_unichar_isalnum (*end) || *end == '_'))
					end++;

				out = g_strndup ((const gchar *) cur, end - cur);

				temp_str = g_strsplit (str, out, 2);

				for (list = clue_list; list; list = g_slist_next (list)) {
					gchar **temp = g_strsplit (list->data, "=", 2);
					if (!g_ascii_strcasecmp(temp[0], out+1)) {
						g_free (str);
						str = g_strconcat (temp_str[0], temp[1], temp_str[1], NULL);
						count1 = 1;
						string_changed = 1;
					} else
						count1 = 0;
					g_strfreev(temp);
				}

				if (!count1) {
					if (getenv (out+1)) {
						g_free (str);
						str = g_strconcat (temp_str[0], getenv (out + 1), temp_str[1], NULL);
						count1 = 1;
						string_changed = 1;
					} else
						count1 = 0;
				}

				if (!count1) {
					const CamelInternetAddress *to;
					const gchar *name, *addr;

					to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
					if (!camel_internet_address_get (to, 0, &name, &addr))
						continue;

					if (name && g_ascii_strcasecmp ("sender_name", out + 1) == 0) {
						g_free (str);
						str = g_strconcat (temp_str[0], name, temp_str[1], NULL);
						count1 = 1;
						string_changed = 1;
					} else if (addr && g_ascii_strcasecmp ("sender_email", out + 1) == 0) {
						g_free (str);
						str = g_strconcat (temp_str[0], addr, temp_str[1], NULL);
						count1 = 1;
						string_changed = 1;
					}
				}

				g_strfreev (temp_str);
				g_free (out);
			}
		}

		if (clue_list) {
			g_slist_foreach (clue_list, (GFunc) g_free, NULL);
			g_slist_free (clue_list);
		}

		if (string_changed) {

			/* Create toplevel container */
			camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (body),
					"multipart/alternative;");
			camel_multipart_set_boundary (body, NULL);

			stream = camel_stream_mem_new ();

			mail_text = camel_data_wrapper_new ();
			camel_data_wrapper_set_mime_type_field (mail_text, type);

			camel_stream_printf (stream, "%s", str);

			camel_data_wrapper_construct_from_stream (mail_text, stream);
			camel_object_unref (stream);

			part = camel_mime_part_new ();
			camel_medium_set_content_object (CAMEL_MEDIUM (part), mail_text);
			camel_object_unref (mail_text);
			camel_multipart_add_part (body, part);
			camel_object_unref (part);

			/* Finish creating the message */
			camel_medium_set_content_object (CAMEL_MEDIUM (message), CAMEL_DATA_WRAPPER(body));
			camel_object_unref (body);
		}

		g_free (str);
	}

	composer = e_msg_composer_new_with_message (message);

	if (em_utils_folder_is_templates(drafts, NULL) == TRUE)
		em_composer_utils_setup_callbacks (composer, NULL, NULL, 0, 0, NULL, NULL);
	else
		em_composer_utils_setup_callbacks (composer, NULL, NULL, 0, 0, drafts, uid);

	composer_set_no_change (composer, TRUE, FALSE);

	if (!e_msg_composer_get_lite())
		gtk_widget_show (GTK_WIDGET (composer));

	return (GtkWidget *)composer;
}

/**
 * em_utils_edit_message:
 * @message: message to edit
 * @folder: used to recognize the templates folder
 *
 * Opens a composer filled in with the headers/mime-parts/etc of
 * @message.
 **/
GtkWidget *
em_utils_edit_message (CamelMimeMessage *message, CamelFolder *folder)
{
	GtkWidget *composer = NULL;
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	if (folder)
		composer = edit_message (message, folder, NULL);
	else
		composer = edit_message (message, NULL, NULL);

	return composer;
}

static void
edit_messages (CamelFolder *folder, GPtrArray *uids, GPtrArray *msgs, gpointer user_data)
{
	gboolean replace = GPOINTER_TO_INT (user_data);
	gint i;

	if (msgs == NULL)
		return;

	for (i = 0; i < msgs->len; i++) {
		camel_medium_remove_header (CAMEL_MEDIUM (msgs->pdata[i]), "X-Mailer");

		if (replace)
			edit_message (msgs->pdata[i], folder, uids->pdata[i]);
		else
			edit_message (msgs->pdata[i], NULL, NULL);
	}
}

/**
 * em_utils_edit_messages:
 * @folder: folder containing messages to edit
 * @uids: uids of messages to edit
 * @replace: replace the existing message(s) when sent or saved.
 *
 * Opens a composer for each message to be edited.
 **/
void
em_utils_edit_messages (CamelFolder *folder, GPtrArray *uids, gboolean replace)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	mail_get_messages (folder, uids, edit_messages, GINT_TO_POINTER (replace));
}

/* Forwarding messages... */
struct forward_attached_data
{
	CamelFolder *folder;
	GPtrArray *uids;
	gchar *fromuri;
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
		camel_object_unref (fad->folder);
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

	camel_object_ref (folder);

	fad = g_new0 (struct forward_attached_data, 1);
	fad->folder = folder;
	fad->uids = em_utils_uids_copy (uids);

	g_signal_connect (composer, "send", G_CALLBACK (update_forwarded_flags_cb), fad);
	g_signal_connect (composer, "save-draft", G_CALLBACK (update_forwarded_flags_cb), fad);

	g_object_weak_ref ((GObject *) composer, (GWeakNotify) composer_destroy_fad_cb, fad);
}

static struct _EMsgComposer *
forward_attached (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, CamelMimePart *part, gchar *subject, const gchar *fromuri)
{
	EMsgComposer *composer;

	composer = create_new_composer (subject, fromuri, TRUE, FALSE);
	if (composer == NULL)
		return NULL;

	e_msg_composer_attach (composer, part);

	if (uids)
		setup_forward_attached_callbacks (composer, folder, uids);

	composer_set_no_change (composer, TRUE, TRUE);

	if (!e_msg_composer_get_lite())
		gtk_widget_show (GTK_WIDGET (composer));

	return composer;
}

static void
forward_attached_cb (CamelFolder *folder, GPtrArray *messages, CamelMimePart *part, gchar *subject, gpointer user_data)
{
	struct forward_attached_data *fad = (struct forward_attached_data *) user_data;

	if (part)
		forward_attached (folder, fad->uids, messages, part, subject, fad->fromuri);

	g_free (fad->fromuri);
	g_free (fad);
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
em_utils_forward_attached (CamelFolder *folder, GPtrArray *uids, const gchar *fromuri)
{
	struct forward_attached_data *fad;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	fad = g_new0 (struct forward_attached_data, 1);
	fad->uids = uids;
	fad->fromuri = g_strdup (fromuri);

	mail_build_attachment (folder, uids, forward_attached_cb, fad);
}

static struct _EMsgComposer *
forward_non_attached (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, gint style, const gchar *fromuri)
{
	CamelMimeMessage *message;
	EMsgComposer *composer = NULL;
	gchar *subject, *text;
	gint i;
	guint32 flags;

	if (messages->len == 0)
		return NULL;

	flags = EM_FORMAT_QUOTE_HEADERS;
	if (style == MAIL_CONFIG_FORWARD_QUOTED)
		flags |= EM_FORMAT_QUOTE_CITE;

	for (i = 0; i < messages->len; i++) {
		gssize len;

		message = messages->pdata[i];
		subject = mail_tool_generate_forward_subject (message);

		text = em_utils_message_to_html (message, _("-------- Forwarded Message --------"), flags, &len, NULL, NULL);

		if (text) {
			composer = create_new_composer (subject, fromuri, !uids || !uids->pdata [i], FALSE);

			if (composer) {
				if (CAMEL_IS_MULTIPART(camel_medium_get_content_object((CamelMedium *)message)))
					e_msg_composer_add_message_attachments(composer, message, FALSE);

				e_msg_composer_set_body_text (composer, text, len);

				if (uids && uids->pdata[i])
					em_composer_utils_setup_callbacks (composer, folder, uids->pdata[i], CAMEL_MESSAGE_FORWARDED, CAMEL_MESSAGE_FORWARDED, NULL, NULL);

				composer_set_no_change (composer, TRUE, TRUE);
				if (!e_msg_composer_get_lite())
					gtk_widget_show (GTK_WIDGET (composer));
			}
			g_free (text);
		}

		g_free (subject);
	}

	return composer;
}

static void
forward_inline (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, gpointer user_data)
{
	forward_non_attached (folder, uids, messages, MAIL_CONFIG_FORWARD_INLINE, (gchar *)user_data);
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
em_utils_forward_inline (CamelFolder *folder, GPtrArray *uids, const gchar *fromuri)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	mail_get_messages (folder, uids, forward_inline, g_strdup(fromuri));
}

static void
forward_quoted (CamelFolder *folder, GPtrArray *uids, GPtrArray *messages, gpointer user_data)
{
	forward_non_attached (folder, uids, messages, MAIL_CONFIG_FORWARD_QUOTED, (gchar *)user_data);
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
em_utils_forward_quoted (CamelFolder *folder, GPtrArray *uids, const gchar *fromuri)
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
struct _EMsgComposer *
em_utils_forward_message (CamelMimeMessage *message, const gchar *fromuri)
{
	GPtrArray *messages;
	CamelMimePart *part;
	GConfClient *gconf;
	gchar *subject;
	gint mode;
	struct _EMsgComposer *composer = NULL;
	messages = g_ptr_array_new ();
	g_ptr_array_add (messages, message);

	gconf = mail_config_get_gconf_client ();
	mode = gconf_client_get_int (gconf, "/apps/evolution/mail/format/forward_style", NULL);

	switch (mode) {
	case MAIL_CONFIG_FORWARD_ATTACHED:
	default:
		part = mail_tool_make_message_attachment (message);

		subject = mail_tool_generate_forward_subject (message);

		composer = forward_attached (NULL, NULL, messages, part, subject, fromuri);
		camel_object_unref (part);
		g_free (subject);
		break;
	case MAIL_CONFIG_FORWARD_INLINE:
		composer = forward_non_attached (NULL, NULL, messages, MAIL_CONFIG_FORWARD_INLINE, fromuri);
		break;
	case MAIL_CONFIG_FORWARD_QUOTED:
		composer = forward_non_attached (NULL, NULL, messages, MAIL_CONFIG_FORWARD_QUOTED, fromuri);
		break;
	}

	g_ptr_array_free (messages, TRUE);

	return composer;
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
em_utils_forward_messages (CamelFolder *folder, GPtrArray *uids, const gchar *fromuri)
{
	GConfClient *gconf;
	gint mode;

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

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	composer = redirect_get_composer (message);

	gtk_widget_show (GTK_WIDGET (composer));

	composer_set_no_change (composer, TRUE, TRUE);
}

static void
redirect_msg (CamelFolder *folder, const gchar *uid, CamelMimeMessage *message, gpointer user_data)
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
em_utils_redirect_message_by_uid (CamelFolder *folder, const gchar *uid)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uid != NULL);

	mail_get_message (folder, uid, redirect_msg, NULL, mail_msg_unordered_push);
}

static void
emu_handle_receipt_message(CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer data, CamelException *ex)
{
	if (msg)
		em_utils_handle_receipt(folder, uid, msg);

	/* we dont care really if we can't get the message */
	camel_exception_clear(ex);
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
		camel_message_info_free(info);
		return;
	}

	if (msg == NULL) {
		mail_get_messagex(folder, uid, emu_handle_receipt_message, NULL, mail_msg_unordered_push);
		camel_message_info_free(info);
		return;
	}

	if ((addr = camel_medium_get_header((CamelMedium *)msg, "Disposition-Notification-To")) == NULL) {
		camel_message_info_free(info);
		return;
	}

	camel_message_info_set_user_flag(info, "receipt-handled", TRUE);
	camel_message_info_free(info);

	account = guess_account(msg, folder);

	/* TODO: should probably decode/format the address, it could be in rfc2047 format */
	if (addr == NULL) {
		addr = "";
	} else {
		while (camel_mime_is_lwsp(*addr))
			addr++;
	}

	if (account && (account->receipt_policy == E_ACCOUNT_RECEIPT_ALWAYS || account->receipt_policy == E_ACCOUNT_RECEIPT_ASK)
	    && e_error_run (NULL, "mail:ask-receipt", addr, camel_mime_message_get_subject(msg), NULL) == GTK_RESPONSE_YES)
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
	EAccount *account = guess_account (message, folder);
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
			     "Your message to %s about \"%s\" on %s has been read.",
			     self_address, message_subject, message_date);
	camel_data_wrapper_construct_from_stream (receipt_text, stream);
	camel_object_unref (stream);

	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), receipt_text);
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE);
	camel_object_unref (receipt_text);
	camel_multipart_add_part (body, part);
	camel_object_unref (part);

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
	camel_data_wrapper_construct_from_stream (receipt_data, stream);
	camel_object_unref (stream);

	g_free (ua);
	g_free (recipient);
	g_free (fake_msgid);

	camel_medium_set_content_object (CAMEL_MEDIUM (part), receipt_data);
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_7BIT);
	camel_object_unref (receipt_data);
	camel_multipart_add_part (body, part);
	camel_object_unref (part);

	/* Finish creating the message */
	camel_medium_set_content_object (CAMEL_MEDIUM (receipt), CAMEL_DATA_WRAPPER (body));
	camel_object_unref (body);

	receipt_subject = g_strdup_printf ("Delivery Notification for: \"%s\"", message_subject);
	camel_mime_message_set_subject (receipt, receipt_subject);
	g_free (receipt_subject);

	addr = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (addr), self_address);
	camel_mime_message_set_from (receipt, addr);
	camel_object_unref (addr);

	addr = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (addr), receipt_address);
	camel_mime_message_set_recipients (receipt, CAMEL_RECIPIENT_TYPE_TO, addr);
	camel_object_unref (addr);

	camel_medium_set_header (CAMEL_MEDIUM (receipt), "Return-Path", "<>");
	if (account) {
		camel_medium_set_header (CAMEL_MEDIUM (receipt),
					"X-Evolution-Account", account->uid);
		camel_medium_set_header (CAMEL_MEDIUM (receipt),
					"X-Evolution-Transport", account->transport->url);
		camel_medium_set_header (CAMEL_MEDIUM (receipt),
					"X-Evolution-Fcc",  account->sent_folder_uri);
	}

	/* Send the receipt */
	out_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX);
	info = camel_message_info_new (NULL);
	camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	mail_append_mail (out_folder, receipt, info, em_utils_receipt_done, NULL);
}

static void
emu_forward_raw_done (CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info,
		       gint queued, const gchar *appended_uid, gpointer data)
{
	camel_message_info_free (info);
	/* do not call mail send, just pile them all in the outbox */
	/* mail_send (); */
}

/**
 * em_utils_forward_message_raw:
 * @param folder Where's a message located.
 * @param message Message to forward.
 * @param address Forward to whom.
 * @param ex Exception.
 * Forwards message to the address, in very similar way as redirect does.
 **/
void
em_utils_forward_message_raw (CamelFolder *folder, CamelMimeMessage *message, const gchar *address, CamelException *ex)
{
	EAccount *account;
	CamelMimeMessage *forward;
	CamelStream *mem;
	CamelInternetAddress *addr;
	CamelFolder *out_folder;
	CamelMessageInfo *info;
	struct _camel_header_raw *xev;
	gchar *subject;

	g_return_if_fail (folder != NULL);
	g_return_if_fail (message != NULL);
	g_return_if_fail (address != NULL);

	if (!*address) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("No destination address provided, forward of the message has been cancelled."));
		return;
	}

	account = guess_account (message, folder);
	if (!account) {
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, _("No account found to use, forward of the message has been cancelled."));
		return;
	}

	forward = camel_mime_message_new ();

	/* make copy of the message, because we are going to modify it */
	mem = camel_stream_mem_new ();
	camel_data_wrapper_write_to_stream ((CamelDataWrapper *)message, mem);
	camel_seekable_stream_seek (CAMEL_SEEKABLE_STREAM (mem), 0, CAMEL_STREAM_SET);
	camel_data_wrapper_construct_from_stream ((CamelDataWrapper *)forward, mem);
	camel_object_unref (mem);

	/* clear previous recipients */
	camel_mime_message_set_recipients (forward, CAMEL_RECIPIENT_TYPE_TO, NULL);
	camel_mime_message_set_recipients (forward, CAMEL_RECIPIENT_TYPE_CC, NULL);
	camel_mime_message_set_recipients (forward, CAMEL_RECIPIENT_TYPE_BCC, NULL);
	camel_mime_message_set_recipients (forward, CAMEL_RECIPIENT_TYPE_RESENT_TO, NULL);
	camel_mime_message_set_recipients (forward, CAMEL_RECIPIENT_TYPE_RESENT_CC, NULL);
	camel_mime_message_set_recipients (forward, CAMEL_RECIPIENT_TYPE_RESENT_BCC, NULL);

	/* remove all delivery and notification headers */
	while (camel_medium_get_header (CAMEL_MEDIUM (forward), "Disposition-Notification-To"))
		camel_medium_remove_header (CAMEL_MEDIUM (forward), "Disposition-Notification-To");

	while (camel_medium_get_header (CAMEL_MEDIUM (forward), "Delivered-To"))
		camel_medium_remove_header (CAMEL_MEDIUM (forward), "Delivered-To");

	/* remove any X-Evolution-* headers that may have been set */
	xev = mail_tool_remove_xevolution_headers (forward);
	camel_header_raw_clear (&xev);

	/* from */
	addr = camel_internet_address_new ();
	camel_internet_address_add (addr, account->id->name, account->id->address);
	camel_mime_message_set_from (forward, addr);
	camel_object_unref (addr);

	/* to */
	addr = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (addr), address);
	camel_mime_message_set_recipients (forward, CAMEL_RECIPIENT_TYPE_TO, addr);
	camel_object_unref (addr);

	/* subject */
	subject = mail_tool_generate_forward_subject (message);
	camel_mime_message_set_subject (forward, subject);
	g_free (subject);

	/* and send it */
	out_folder = mail_component_get_folder (NULL, MAIL_COMPONENT_FOLDER_OUTBOX);
	info = camel_message_info_new (NULL);
	camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	mail_append_mail (out_folder, forward, info, emu_forward_raw_done, NULL);
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

	def = mail_config_get_default_account ();

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
				g_hash_table_insert (account_hash, (gchar *) account->id->address, (gpointer) account);
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	/* The default account has to be there if none of the enabled accounts are present */
	if (g_hash_table_size (account_hash) == 0 && def && def->id->address)
		g_hash_table_insert (account_hash, (gchar *) def->id->address, (gpointer) def);

	return account_hash;
}

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
reply_get_composer (CamelMimeMessage *message, EAccount *account,
		    CamelInternetAddress *to, CamelInternetAddress *cc,
		    CamelFolder *folder, CamelNNTPAddress *postto)
{
	const gchar *message_id, *references;
	EDestination **tov, **ccv;
	EMsgComposer *composer;
	EComposerHeaderTable *table;
	gchar *subject;

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);
	g_return_val_if_fail (to == NULL || CAMEL_IS_INTERNET_ADDRESS (to), NULL);
	g_return_val_if_fail (cc == NULL || CAMEL_IS_INTERNET_ADDRESS (cc), NULL);

	composer = e_msg_composer_new ();

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

	g_free (subject);

	/* add post-to, if nessecary */
	if (postto && camel_address_length((CamelAddress *)postto)) {
		gchar *store_url = NULL;
		gchar *post;

		if (folder) {
			store_url = camel_url_to_string (CAMEL_SERVICE (folder->parent_store)->url, CAMEL_URL_HIDE_ALL);
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

static EAccount *
guess_account_folder(CamelFolder *folder)
{
	EAccount *account;
	gchar *tmp;

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
	const gchar *tmp;
	gint i, j;
	const gchar *types[2] = {
		CAMEL_RECIPIENT_TYPE_TO,
		CAMEL_RECIPIENT_TYPE_CC
	};

	/* check for newsgroup header */
	if (folder
	    && camel_medium_get_header((CamelMedium *)message, "Newsgroups")
	    && (account = guess_account_folder(folder)))
		return account;

	/* check for source folder */
	if (folder) {
		account = guess_account_folder(folder);
		if (account)
			return account;
	}

	/* then message source */
	if (account == NULL
	    && (tmp = camel_mime_message_get_source(message))) {
		account = mail_config_get_account_by_source_url(tmp);
		if (account)
			return account;
	}

	/* finally recipient (to/cc) in account table */
	account_hash = generate_account_hash ();
	for (j=0;account == NULL && j<2;j++) {
		const CamelInternetAddress *to;

		to = camel_mime_message_get_recipients(message, types[j]);
		if (to) {
			for (i = 0; camel_internet_address_get(to, i, NULL, &tmp); i++) {
				account = g_hash_table_lookup(account_hash, tmp);
				if (account && account->enabled)
					break;
			}
		}
	}
	g_hash_table_destroy(account_hash);

	return account;
}

static void
get_reply_sender (CamelMimeMessage *message, CamelInternetAddress *to, CamelNNTPAddress *postto)
{
	const CamelInternetAddress *reply_to;
	const gchar *name, *addr, *posthdr;
	gint i;

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto
	    && ((posthdr = camel_medium_get_header((CamelMedium *)message, "Followup-To"))
		 || (posthdr = camel_medium_get_header((CamelMedium *)message, "Newsgroups")))) {
		camel_address_decode((CamelAddress *)postto, posthdr);
		return;
	}

	reply_to = camel_mime_message_get_reply_to (message);
	if (!reply_to)
		reply_to = camel_mime_message_get_from (message);

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

static void
concat_unique_addrs (CamelInternetAddress *dest, const CamelInternetAddress *src, GHashTable *rcpt_hash)
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
	const CamelInternetAddress *reply_to, *to_addrs, *cc_addrs;
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

	rcpt_hash = generate_account_hash ();

	reply_to = camel_mime_message_get_reply_to (message);
	if (!reply_to)
		reply_to = camel_mime_message_get_from (message);

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
	const CamelInternetAddress *sender;
	const gchar *name, *addr;

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

	start = inptr = format;
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
		camel_object_unref (part);
		break;
	case MAIL_CONFIG_REPLY_OUTLOOK:
		text = em_utils_message_to_html (message, _("-----Original Message-----"), EM_FORMAT_QUOTE_HEADERS, &len, source, start_bottom ? "<BR>" : NULL);
		e_msg_composer_set_body_text(composer, text, len);
		g_free (text);
		break;

	case MAIL_CONFIG_REPLY_QUOTED:
	default:
		/* do what any sane user would want when replying... */
		credits = attribution_format (ATTRIBUTION, message);
		text = em_utils_message_to_html (message, credits, EM_FORMAT_QUOTE_CITE, &len, source, start_bottom ? "<BR>" : NULL);
		g_free (credits);
		e_msg_composer_set_body_text(composer, text, len);
		g_free (text);
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
	text = em_utils_message_to_html (message, credits, EM_FORMAT_QUOTE_CITE, &len, source, start_bottom ? "<BR>" : NULL);

	g_free (credits);
	return text;
}

static void
reply_to_message(CamelFolder *folder, const gchar *uid, CamelMimeMessage *message, gpointer user_data)
{
	struct _reply_data *rd = user_data;

	if (message != NULL)
		em_utils_reply_to_message(folder, uid, message, rd->mode, rd->source);

	if (rd->source)
		g_object_unref(rd->source);
	g_free(rd);
}

/**
 * em_utils_reply_to_message:
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
 * been replied to.
 **/
struct _EMsgComposer *
em_utils_reply_to_message(CamelFolder *folder, const gchar *uid, CamelMimeMessage *message, gint mode, EMFormat *source)
{
	CamelInternetAddress *to, *cc;
	CamelNNTPAddress *postto = NULL;
	EMsgComposer *composer;
	EAccount *account;
	guint32 flags;
	EMEvent *eme;
	EMEventTargetMessage *target;

	if (folder && uid && message == NULL) {
		struct _reply_data *rd = g_malloc0(sizeof(*rd));

		rd->mode = mode;
		rd->source = source;
		if (rd->source)
			g_object_ref(rd->source);
		mail_get_message(folder, uid, reply_to_message, rd, mail_msg_unordered_push);

		return NULL;
	}

	g_return_val_if_fail(message != NULL, NULL);

	to = camel_internet_address_new();
	cc = camel_internet_address_new();

	account = guess_account (message, folder);
	flags = CAMEL_MESSAGE_ANSWERED | CAMEL_MESSAGE_SEEN;

	switch (mode) {
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

	composer = reply_get_composer (message, account, to, cc, folder, postto);
	e_msg_composer_add_message_attachments (composer, message, TRUE);

	if (postto)
		camel_object_unref(postto);
	camel_object_unref(to);
	camel_object_unref(cc);

	composer_set_body (composer, message, source);

	/** @Event: message.replying
	 * @Title: Message being replied to
	 * @Target: EMEventTargetMessage
	 *
	 * message.replying is emitted when a user starts replying to a message.
	 */

	eme = em_event_peek();
	target = em_event_target_new_message(eme, folder, message, uid,
					     mode == REPLY_MODE_ALL ? EM_EVENT_MESSAGE_REPLY_ALL | EM_EVENT_MESSAGE_REPLY : EM_EVENT_MESSAGE_REPLY, composer);
	e_event_emit((EEvent *)eme, "message.replying", (EEventTarget *)target);

	em_composer_utils_setup_callbacks (composer, folder, uid, flags, flags, NULL, NULL);

	composer_set_no_change (composer, TRUE, TRUE);

	if (!e_msg_composer_get_lite())
		gtk_widget_show (GTK_WIDGET (composer));

	return composer;
}
