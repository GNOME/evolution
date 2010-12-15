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
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

/* mail-session.c: handles the session information and resource manipulation */

#include <config.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#ifdef HAVE_CANBERRA
#include <canberra-gtk.h>
#endif

#include <libedataserverui/e-passwords.h>
#include <libedataserver/e-flag.h>

#include "e-util/e-util.h"
#include "e-util/e-alert-dialog.h"
#include "e-util/e-util-private.h"

#include "em-composer-utils.h"
#include "em-filter-context.h"
#include "em-filter-rule.h"
#include "em-utils.h"
#include "e-mail-local.h"
#include "mail-config.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-send-recv.h"
#include "mail-session.h"
#include "mail-tools.h"

#define d(x)

CamelSession *session;
static guint session_check_junk_notify_id;
static guint session_gconf_proxy_id;

#define MAIL_TYPE_SESSION \
	(mail_session_get_type ())

typedef struct _MailSession MailSession;
typedef struct _MailSessionClass MailSessionClass;

struct _MailSession {
	CamelSession parent_object;

	gboolean interactive;
	FILE *filter_logfile;
	GList *junk_plugins;

	MailAsyncEvent *async;
};

struct _MailSessionClass {
	CamelSessionClass parent_class;
};

static gchar *mail_data_dir;
static gchar *mail_config_dir;

static gchar *get_password(CamelSession *session, CamelService *service, const gchar *domain, const gchar *prompt, const gchar *item, guint32 flags, GError **error);
static gboolean forget_password(CamelSession *session, CamelService *service, const gchar *domain, const gchar *item, GError **error);
static gboolean alert_user(CamelSession *session, CamelSessionAlertType type, const gchar *prompt, gboolean cancel);
static CamelFilterDriver *get_filter_driver(CamelSession *session, const gchar *type, GError **error);
static gboolean lookup_addressbook(CamelSession *session, const gchar *name);

static void ms_thread_status(CamelSession *session, CamelSessionThreadMsg *msg, const gchar *text, gint pc);
static gpointer ms_thread_msg_new(CamelSession *session, CamelSessionThreadOps *ops, guint size);
static void ms_thread_msg_free(CamelSession *session, CamelSessionThreadMsg *m);
static gboolean ms_forward_to (CamelSession *session, CamelFolder *folder, CamelMimeMessage *message, const gchar *address, GError **error);

GType mail_session_get_type (void);

G_DEFINE_TYPE (MailSession, mail_session, CAMEL_TYPE_SESSION)

static void
mail_session_finalize (GObject *object)
{
	MailSession *session = (MailSession *) object;
	GConfClient *client;

	client = mail_config_get_gconf_client ();

	if (session_check_junk_notify_id != 0) {
		gconf_client_notify_remove (client, session_check_junk_notify_id);
		session_check_junk_notify_id = 0;
	}

	if (session_gconf_proxy_id != 0) {
		gconf_client_notify_remove (client, session_gconf_proxy_id);
		session_gconf_proxy_id = 0;
	}

	mail_async_event_destroy(session->async);

	g_free (mail_data_dir);
	g_free (mail_config_dir);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (mail_session_parent_class)->finalize (object);
}

static void
mail_session_class_init (MailSessionClass *class)
{
	GObjectClass *object_class;
	CamelSessionClass *session_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = mail_session_finalize;

	session_class = CAMEL_SESSION_CLASS (class);
	session_class->get_password = get_password;
	session_class->forget_password = forget_password;
	session_class->alert_user = alert_user;
	session_class->get_filter_driver = get_filter_driver;
	session_class->lookup_addressbook = lookup_addressbook;
	session_class->thread_msg_new = ms_thread_msg_new;
	session_class->thread_msg_free = ms_thread_msg_free;
	session_class->thread_status = ms_thread_status;
	session_class->forward_to = ms_forward_to;
}

static void
mail_session_init (MailSession *session)
{
	session->async = mail_async_event_new();
	session->junk_plugins = NULL;
}

static gchar *
make_key (CamelService *service, const gchar *item)
{
	gchar *key;

	if (service)
		key = camel_url_to_string (service->url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	else
		key = g_strdup (item);

	return key;
}

/* ********************************************************************** */

static gchar *
get_password (CamelSession *session,
              CamelService *service,
              const gchar *domain,
              const gchar *prompt,
              const gchar *item,
              guint32 flags,
              GError **error)
{
	gchar *url;
	gchar *ret = NULL;
	EAccount *account = NULL;

	url = service?camel_url_to_string(service->url, CAMEL_URL_HIDE_ALL):NULL;

	if (!strcmp(item, "popb4smtp_uri")) {
		/* not 100% mt safe, but should be ok */
		if (url
		    && (account = mail_config_get_account_by_transport_url(url)))
			ret = g_strdup(account->source->url);
		else
			ret = g_strdup(url);
	} else {
		gchar *key = make_key(service, item);
		EAccountService *config_service = NULL;

		if (domain == NULL)
			domain = "Mail";

		ret = e_passwords_get_password(domain, key);
		if (ret == NULL || (flags & CAMEL_SESSION_PASSWORD_REPROMPT)) {
			gboolean remember;

			g_free (ret);
			ret = NULL;

			if (url) {
				if  ((account = mail_config_get_account_by_source_url(url)))
					config_service = account->source;
				else if ((account = mail_config_get_account_by_transport_url(url)))
					config_service = account->transport;
			}

			remember = config_service?config_service->save_passwd:FALSE;

			if (!config_service || (config_service && !config_service->get_password_canceled)) {
				guint32 eflags;
				gchar *title;

				if (flags & CAMEL_SESSION_PASSPHRASE) {
					if (account)
						title = g_strdup_printf (_("Enter Passphrase for %s"), account->name);
					else
						title = g_strdup (_("Enter Passphrase"));
				} else {
					if (account)
						title = g_strdup_printf (_("Enter Password for %s"), account->name);
					else
						title = g_strdup (_("Enter Password"));
				}
				if ((flags & CAMEL_SESSION_PASSWORD_STATIC) != 0)
					eflags = E_PASSWORDS_REMEMBER_NEVER;
				else if (config_service == NULL)
					eflags = E_PASSWORDS_REMEMBER_SESSION;
				else
					eflags = E_PASSWORDS_REMEMBER_FOREVER;

				if (flags & CAMEL_SESSION_PASSWORD_REPROMPT)
					eflags |= E_PASSWORDS_REPROMPT;

				if (flags & CAMEL_SESSION_PASSWORD_SECRET)
					eflags |= E_PASSWORDS_SECRET;

				if (flags & CAMEL_SESSION_PASSPHRASE)
					eflags |= E_PASSWORDS_PASSPHRASE;

				/* HACK: breaks abstraction ...
				   e_account_writable doesn't use the eaccount, it also uses the same writable key for
				   source and transport */
				if (!e_account_writable(NULL, E_ACCOUNT_SOURCE_SAVE_PASSWD))
					eflags |= E_PASSWORDS_DISABLE_REMEMBER;

				ret = e_passwords_ask_password(title, domain, key, prompt, eflags, &remember, NULL);

				if (!ret)
					e_passwords_forget_password (domain, key);

				g_free(title);

				if (ret && config_service)
					mail_config_service_set_save_passwd(config_service, remember);

				if (config_service)
					config_service->get_password_canceled = ret == NULL;
			}
		}

		g_free(key);
	}

	g_free(url);

	if (ret == NULL)
		g_set_error (
			error, G_IO_ERROR,
			G_IO_ERROR_CANCELLED,
			_("User canceled operation."));

	return ret;
}

static gboolean
forget_password (CamelSession *session,
                 CamelService *service,
                 const gchar *domain,
                 const gchar *item,
                 GError **error)
{
	gchar *key = make_key (service, item);

	e_passwords_forget_password (domain?domain:"Mail", key);
	g_free (key);

	return TRUE;
}

/* ********************************************************************** */

static gpointer user_message_dialog;
static GQueue user_message_queue = { NULL, NULL, 0 };

struct _user_message_msg {
	MailMsg base;

	CamelSessionAlertType type;
	gchar *prompt;
	EFlag *done;

	guint allow_cancel:1;
	guint result:1;
	guint ismain:1;
};

static void user_message_exec (struct _user_message_msg *m);

static void
user_message_response_free (GtkDialog *dialog, gint button, struct _user_message_msg *m)
{
	gtk_widget_destroy ((GtkWidget *) dialog);

	user_message_dialog = NULL;

	/* check for pendings */
	if (!g_queue_is_empty (&user_message_queue)) {
		m = g_queue_pop_head (&user_message_queue);
		user_message_exec (m);
		mail_msg_unref (m);
	}
}

/* clicked, send back the reply */
static void
user_message_response (GtkDialog *dialog, gint button, struct _user_message_msg *m)
{
	/* if !allow_cancel, then we've already replied */
	if (m->allow_cancel) {
		m->result = button == GTK_RESPONSE_OK;
		e_flag_set (m->done);
	}

	user_message_response_free (dialog, button, m);
}

static void
user_message_exec (struct _user_message_msg *m)
{
	GtkWindow *parent;
	const gchar *error_type;

	if (!m->ismain && user_message_dialog != NULL) {
		g_queue_push_tail (&user_message_queue, mail_msg_ref (m));
		return;
	}

	switch (m->type) {
		case CAMEL_SESSION_ALERT_INFO:
			error_type = m->allow_cancel ?
				"mail:session-message-info-cancel" :
				"mail:session-message-info";
			break;
		case CAMEL_SESSION_ALERT_WARNING:
			error_type = m->allow_cancel ?
				"mail:session-message-warning-cancel" :
				"mail:session-message-warning";
			break;
		case CAMEL_SESSION_ALERT_ERROR:
			error_type = m->allow_cancel ?
				"mail:session-message-error-cancel" :
				"mail:session-message-error";
			break;
		default:
			error_type = NULL;
			g_return_if_reached ();
	}

	/* Pull in the active window from the shell to get a parent window */
	parent = e_shell_get_active_window (e_shell_get_default ());
	user_message_dialog =
		e_alert_dialog_new_for_args (parent, error_type, m->prompt, NULL);
	g_object_set (
		user_message_dialog, "allow_shrink", TRUE,
		"allow_grow", TRUE, NULL);

	/* Use the number of dialog buttons as a heuristic for whether to
	 * emit a status bar message or present the dialog immediately, the
	 * thought being if there's more than one button then something is
	 * probably blocked until the user responds. */
	if (e_alert_dialog_count_buttons (user_message_dialog) > 1) {
		if (m->ismain) {
			gint response;

			response = gtk_dialog_run (user_message_dialog);
			user_message_response (
				user_message_dialog, response, m);
		} else {
			g_signal_connect (
				user_message_dialog, "response",
				G_CALLBACK (user_message_response), m);
			gtk_widget_show (user_message_dialog);
		}
	} else {
		g_signal_connect (
			user_message_dialog, "response",
			G_CALLBACK (user_message_response_free), m);
		g_object_set_data (
			user_message_dialog, "response-handled",
			GINT_TO_POINTER (TRUE));
		em_utils_show_error_silent (user_message_dialog);
	}
}

static void
user_message_free (struct _user_message_msg *m)
{
	g_free (m->prompt);
	e_flag_free (m->done);
}

static MailMsgInfo user_message_info = {
	sizeof (struct _user_message_msg),
	(MailMsgDescFunc) NULL,
	(MailMsgExecFunc) user_message_exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) user_message_free
};

static gboolean
lookup_addressbook(CamelSession *session, const gchar *name)
{
	CamelInternetAddress *addr;
	gboolean ret;

	if (!mail_config_get_lookup_book ())
		return FALSE;

	addr = camel_internet_address_new ();
	camel_address_decode ((CamelAddress *)addr, name);
	ret = em_utils_in_addressbook (addr, mail_config_get_lookup_book_local_only ());
	g_object_unref (addr);

	return ret;
}

static gboolean
alert_user(CamelSession *session, CamelSessionAlertType type, const gchar *prompt, gboolean cancel)
{
	struct _user_message_msg *m;
	gboolean result = TRUE;

	m = mail_msg_new (&user_message_info);
	m->ismain = mail_in_main_thread ();
	m->type = type;
	m->prompt = g_strdup (prompt);
	m->done = e_flag_new ();
	m->allow_cancel = cancel;

	if (cancel)
		mail_msg_ref (m);

	if (m->ismain)
		user_message_exec (m);
	else
		mail_msg_main_loop_push (m);

	if (cancel) {
		e_flag_wait (m->done);
		result = m->result;
		mail_msg_unref (m);
	} else if (m->ismain)
		mail_msg_unref (m);

	return result;
}

static CamelFolder *
get_folder (CamelFilterDriver *d,
            const gchar *uri,
            gpointer data,
            GError **error)
{
	return mail_tool_uri_to_folder (uri, 0, error);
}

static void
main_play_sound (CamelFilterDriver *driver, gchar *filename, gpointer user_data)
{
	if (filename && *filename) {
#ifdef HAVE_CANBERRA
		ca_context_play(ca_gtk_context_get(), 0,
				CA_PROP_MEDIA_FILENAME, filename,
				NULL);
#endif
	} else
		gdk_beep ();

	g_free (filename);
	g_object_unref (session);
}

static void
session_play_sound (CamelFilterDriver *driver, const gchar *filename, gpointer user_data)
{
	MailSession *ms = (MailSession *) session;

	g_object_ref (session);

	mail_async_event_emit (ms->async, MAIL_ASYNC_GUI, (MailAsyncFunc) main_play_sound,
			       driver, g_strdup (filename), user_data);
}

static void
main_system_beep (CamelFilterDriver *driver, gpointer user_data)
{
	gdk_beep ();
	g_object_unref (session);
}

static void
session_system_beep (CamelFilterDriver *driver, gpointer user_data)
{
	MailSession *ms = (MailSession *) session;

	g_object_ref (session);

	mail_async_event_emit (ms->async, MAIL_ASYNC_GUI, (MailAsyncFunc) main_system_beep,
			       driver, user_data, NULL);
}

static CamelFilterDriver *
main_get_filter_driver (CamelSession *session, const gchar *type, GError **error)
{
	CamelFilterDriver *driver;
	EFilterRule *rule = NULL;
	const gchar *config_dir;
	gchar *user, *system;
	GConfClient *gconf;
	ERuleContext *fc;

	gconf = mail_config_get_gconf_client ();

	config_dir = mail_session_get_config_dir ();
	user = g_build_filename (config_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	fc = (ERuleContext *) em_filter_context_new ();
	e_rule_context_load (fc, system, user);
	g_free (system);
	g_free (user);

	driver = camel_filter_driver_new (session);
	camel_filter_driver_set_folder_func (driver, get_folder, NULL);

	if (gconf_client_get_bool (gconf, "/apps/evolution/mail/filters/log", NULL)) {
		MailSession *ms = (MailSession *) session;

		if (ms->filter_logfile == NULL) {
			gchar *filename;

			filename = gconf_client_get_string (gconf, "/apps/evolution/mail/filters/logfile", NULL);
			if (filename) {
				ms->filter_logfile = g_fopen (filename, "a+");
				g_free (filename);
			}
		}

		if (ms->filter_logfile)
			camel_filter_driver_set_logfile (driver, ms->filter_logfile);
	}

	camel_filter_driver_set_shell_func (driver, mail_execute_shell_command, NULL);
	camel_filter_driver_set_play_sound_func (driver, session_play_sound, NULL);
	camel_filter_driver_set_system_beep_func (driver, session_system_beep, NULL);

	if ((!strcmp (type, E_FILTER_SOURCE_INCOMING) || !strcmp (type, E_FILTER_SOURCE_JUNKTEST))
	    && camel_session_get_check_junk (session)) {
		/* implicit junk check as 1st rule */
		camel_filter_driver_add_rule (driver, "Junk check", "(junk-test)", "(begin (set-system-flag \"junk\"))");
	}

	if (strcmp (type, E_FILTER_SOURCE_JUNKTEST) != 0) {
		GString *fsearch, *faction;

		fsearch = g_string_new ("");
		faction = g_string_new ("");

		if (!strcmp (type, E_FILTER_SOURCE_DEMAND))
			type = E_FILTER_SOURCE_INCOMING;

		/* add the user-defined rules next */
		while ((rule = e_rule_context_next_rule (fc, rule, type))) {
			g_string_truncate (fsearch, 0);
			g_string_truncate (faction, 0);

			/* skip disabled rules */
			if (!rule->enabled)
				continue;

			e_filter_rule_build_code (rule, fsearch);
			em_filter_rule_build_action ((EMFilterRule *) rule, faction);
			camel_filter_driver_add_rule (driver, rule->name, fsearch->str, faction->str);
		}

		g_string_free (fsearch, TRUE);
		g_string_free (faction, TRUE);
	}

	g_object_unref (fc);

	return driver;
}

static CamelFilterDriver *
get_filter_driver (CamelSession *session, const gchar *type, GError **error)
{
	return (CamelFilterDriver *) mail_call_main (
		MAIL_CALL_p_ppp, (MailMainFunc) main_get_filter_driver,
		session, type, error);
}

/* TODO: This is very temporary, until we have a better way to do the progress reporting,
   we just borrow a dummy mail-mt thread message and hook it onto out camel thread message */

static MailMsgInfo ms_thread_info_dummy = { sizeof (MailMsg) };

static gpointer ms_thread_msg_new(CamelSession *session, CamelSessionThreadOps *ops, guint size)
{
	CamelSessionThreadMsg *msg;
	CamelSessionClass *session_class;

	session_class = CAMEL_SESSION_CLASS (mail_session_parent_class);
	msg = session_class->thread_msg_new (session, ops, size);

	/* We create a dummy mail_msg, and then copy its cancellation port over to ours, so
	   we get cancellation and progress in common with hte existing mail code, for free */
	if (msg) {
		MailMsg *m = mail_msg_new(&ms_thread_info_dummy);

		msg->data = m;
		camel_operation_unref(msg->op);
		msg->op = m->cancel;
		camel_operation_ref(msg->op);
	}

	return msg;
}

static void
ms_thread_msg_free(CamelSession *session, CamelSessionThreadMsg *m)
{
	CamelSessionClass *session_class;

	session_class = CAMEL_SESSION_CLASS (mail_session_parent_class);

	mail_msg_unref(m->data);
	session_class->thread_msg_free(session, m);
}

static void
ms_thread_status(CamelSession *session, CamelSessionThreadMsg *msg, const gchar *text, gint pc)
{
	/* This should never be called since we bypass it in alloc! */
	printf("Thread status '%s' %d%%\n", text, pc);
}

static gboolean
forward_to_flush_outbox_cb (gpointer data)
{
	guint *preparing_flush = data;

	g_return_val_if_fail (preparing_flush != NULL, FALSE);

	*preparing_flush = 0;
	mail_send ();

	return FALSE;
}

static void
ms_forward_to_cb (CamelFolder *folder,
                  CamelMimeMessage *msg,
                  CamelMessageInfo *info,
                  gint queued,
                  const gchar *appended_uid,
                  gpointer data)
{
	static guint preparing_flush = 0;

	camel_message_info_free (info);

	/* do not call mail send immediately, just pile them all in the outbox */
	if (preparing_flush ||
	    gconf_client_get_bool (mail_config_get_gconf_client (), "/apps/evolution/mail/filters/flush-outbox", NULL)) {
		if (preparing_flush)
			g_source_remove (preparing_flush);

		preparing_flush = g_timeout_add_seconds (60, forward_to_flush_outbox_cb, &preparing_flush);
	}
}

static gboolean
ms_forward_to (CamelSession *session,
               CamelFolder *folder,
               CamelMimeMessage *message,
               const gchar *address,
               GError **error)
{
	EAccount *account;
	CamelMimeMessage *forward;
	CamelStream *mem;
	CamelInternetAddress *addr;
	CamelFolder *out_folder;
	CamelMessageInfo *info;
	struct _camel_header_raw *xev;
	gchar *subject;

	g_return_val_if_fail (folder != NULL, FALSE);
	g_return_val_if_fail (message != NULL, FALSE);
	g_return_val_if_fail (address != NULL, FALSE);

	if (!*address) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("No destination address provided, forward "
			  "of the message has been cancelled."));
		return FALSE;
	}

	account = em_utils_guess_account_with_recipients (message, folder);
	if (!account) {
		g_set_error (
			error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			_("No account found to use, forward of the "
			  "message has been cancelled."));
		return FALSE;
	}

	forward = camel_mime_message_new ();

	/* make copy of the message, because we are going to modify it */
	mem = camel_stream_mem_new ();
	camel_data_wrapper_write_to_stream ((CamelDataWrapper *)message, mem, NULL);
	camel_seekable_stream_seek (CAMEL_SEEKABLE_STREAM (mem), 0, CAMEL_STREAM_SET, NULL);
	camel_data_wrapper_construct_from_stream ((CamelDataWrapper *)forward, mem, NULL);
	g_object_unref (mem);

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
	g_object_unref (addr);

	/* to */
	addr = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (addr), address);
	camel_mime_message_set_recipients (forward, CAMEL_RECIPIENT_TYPE_TO, addr);
	g_object_unref (addr);

	/* subject */
	subject = mail_tool_generate_forward_subject (message);
	camel_mime_message_set_subject (forward, subject);
	g_free (subject);

	/* and send it */
	info = camel_message_info_new (NULL);
	out_folder = e_mail_local_get_folder (E_MAIL_FOLDER_OUTBOX);
	camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	mail_append_mail (out_folder, forward, info, ms_forward_to_cb, NULL);

	return TRUE;
}

gchar *
mail_session_get_password (const gchar *url_string)
{
	CamelURL *url;
	gchar *simple_url;
	gchar *passwd;

	url = camel_url_new (url_string, NULL);
	simple_url = camel_url_to_string (url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	camel_url_free (url);

	passwd = e_passwords_get_password ("Mail", simple_url);

	g_free (simple_url);

	return passwd;
}

void
mail_session_add_password (const gchar *url_string,
			   const gchar *passwd)
{
	CamelURL *url;
	gchar *simple_url;

	url = camel_url_new (url_string, NULL);
	simple_url = camel_url_to_string (url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	camel_url_free (url);

	e_passwords_add_password (simple_url, passwd);

	g_free (simple_url);
}

void
mail_session_remember_password (const gchar *url_string)
{
	CamelURL *url;
	gchar *simple_url;

	url = camel_url_new (url_string, NULL);
	simple_url = camel_url_to_string (url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	camel_url_free (url);

	e_passwords_remember_password ("Mail", simple_url);

	g_free (simple_url);
}

void
mail_session_forget_password (const gchar *key)
{
	e_passwords_forget_password ("Mail", key);
}

static void
mail_session_check_junk_notify (GConfClient *gconf, guint id, GConfEntry *entry, CamelSession *session)
{
	gchar *key;

	g_return_if_fail (gconf_entry_get_key (entry) != NULL);
	g_return_if_fail (gconf_entry_get_value (entry) != NULL);

	key = strrchr (gconf_entry_get_key (entry), '/');
	if (key) {
		key++;
		if (!strcmp (key, "check_incoming"))
			camel_session_set_check_junk (session, gconf_value_get_bool (gconf_entry_get_value (entry)));
	}
}

#define DIR_PROXY "/system/proxy"
#define MODE_PROXY "/system/proxy/mode"
#define KEY_SOCKS_HOST "/system/proxy/socks_host"
#define KEY_SOCKS_PORT "/system/proxy/socks_port"

static void
set_socks_proxy_from_gconf (void)
{
	GConfClient *client;
	gchar *mode, *host;
	gint port;

	client = mail_config_get_gconf_client ();

	mode = gconf_client_get_string (client, MODE_PROXY, NULL);
	if (!g_strcmp0(mode, "manual")) {
		host = gconf_client_get_string (client, KEY_SOCKS_HOST, NULL); /* NULL-GError */
		port = gconf_client_get_int (client, KEY_SOCKS_PORT, NULL); /* NULL-GError */
		camel_session_set_socks_proxy (session, host, port);
		g_free (host);
	}
	g_free (mode);
}

static void
proxy_gconf_notify_cb (GConfClient* client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
	const gchar *key;

	key = gconf_entry_get_key (entry);

	if (strcmp (entry->key, KEY_SOCKS_HOST) == 0
	    || strcmp (entry->key, KEY_SOCKS_PORT) == 0)
		set_socks_proxy_from_gconf ();
}

static void
set_socks_proxy_gconf_watch (void)
{
	GConfClient *client;

	client = mail_config_get_gconf_client ();

	gconf_client_add_dir (client, DIR_PROXY, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL); /* NULL-GError */
	session_gconf_proxy_id = gconf_client_notify_add (client, DIR_PROXY, proxy_gconf_notify_cb, NULL, NULL, NULL); /* NULL-GError */
}

static void
init_socks_proxy (void)
{
	set_socks_proxy_gconf_watch ();
	set_socks_proxy_from_gconf ();
}

void
mail_session_start (void)
{
	GConfClient *gconf;

	if (camel_init (e_get_user_data_dir (), TRUE) != 0)
		exit (0);

	camel_provider_init();

	session = g_object_new (MAIL_TYPE_SESSION, NULL);
	e_account_writable(NULL, E_ACCOUNT_SOURCE_SAVE_PASSWD); /* Init the EAccount Setup */

	camel_session_construct (session, mail_session_get_data_dir ());

	gconf = mail_config_get_gconf_client ();
	gconf_client_add_dir (gconf, "/apps/evolution/mail/junk", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	camel_session_set_check_junk (session, gconf_client_get_bool (gconf, "/apps/evolution/mail/junk/check_incoming", NULL));
	session_check_junk_notify_id = gconf_client_notify_add (gconf, "/apps/evolution/mail/junk",
								(GConfClientNotifyFunc) mail_session_check_junk_notify,
								session, NULL, NULL);
	session->junk_plugin = NULL;

	mail_config_reload_junk_headers ();

	init_socks_proxy ();
}

void
mail_session_shutdown (void)
{
	camel_shutdown ();
}

void
mail_session_flush_filter_log (void)
{
	MailSession *ms = (MailSession *) session;

	if (ms->filter_logfile)
		fflush (ms->filter_logfile);
}

void
mail_session_add_junk_plugin (const gchar *plugin_name, CamelJunkPlugin *junk_plugin)
{
	MailSession *ms = (MailSession *) session;
	GConfClient *gconf;
	gchar *def_plugin;

	gconf = mail_config_get_gconf_client ();
	def_plugin = gconf_client_get_string (gconf, "/apps/evolution/mail/junk/default_plugin", NULL);

	ms->junk_plugins = g_list_append(ms->junk_plugins, junk_plugin);
	if (def_plugin && plugin_name) {
		if (!strcmp(def_plugin, plugin_name)) {
			d(printf ("Loading %s as the default junk plugin\n", def_plugin));
			session->junk_plugin = junk_plugin;
			camel_junk_plugin_init (junk_plugin);
		}
	}

	g_free (def_plugin);
}

const GList *
mail_session_get_junk_plugins (void)
{
	MailSession *ms = (MailSession *) session;
	return ms->junk_plugins;
}

void
mail_session_set_junk_headers (const gchar **name, const gchar **value, gint len)
{
	if (!session)
		return;

	camel_session_set_junk_headers (session, name, value, len);
}

const gchar *
mail_session_get_data_dir (void)
{
	if (G_UNLIKELY (mail_data_dir == NULL))
		mail_data_dir = g_build_filename (
			e_get_user_data_dir (), "mail", NULL);

	return mail_data_dir;
}

const gchar *
mail_session_get_config_dir (void)
{
	if (G_UNLIKELY (mail_config_dir == NULL))
		mail_config_dir = g_build_filename (
			e_get_user_config_dir (), "mail", NULL);

	return mail_config_dir;
}

