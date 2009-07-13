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
 *
 */

/* mail-session.c: handles the session information and resource manipulation */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include <libgnome/gnome-sound.h>

#include <libedataserverui/e-passwords.h>
#include <libedataserver/e-flag.h>

#include <camel/camel.h>	/* FIXME: this is where camel_init is defined, it shouldn't include everything else */
#include <camel/camel-filter-driver.h>
#include <camel/camel-i18n.h>

#include "e-util/e-error.h"
#include "e-util/e-util-private.h"
#include "e-account-combo-box.h"

#include "em-composer-utils.h"
#include "em-filter-context.h"
#include "em-filter-rule.h"
#include "em-utils.h"
#include "mail-component.h"
#include "mail-config.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-session.h"
#include "mail-tools.h"

#define d(x)

CamelSession *session;
static gint session_check_junk_notify_id = -1;

#define MAIL_SESSION_TYPE     (mail_session_get_type ())
#define MAIL_SESSION(obj)     (CAMEL_CHECK_CAST((obj), MAIL_SESSION_TYPE, MailSession))
#define MAIL_SESSION_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), MAIL_SESSION_TYPE, MailSessionClass))
#define MAIL_IS_SESSION(o)    (CAMEL_CHECK_TYPE((o), MAIL_SESSION_TYPE))

typedef struct _MailSession {
	CamelSession parent_object;

	gboolean interactive;
	FILE *filter_logfile;
	GList *junk_plugins;

	MailAsyncEvent *async;
} MailSession;

typedef struct _MailSessionClass {
	CamelSessionClass parent_class;

} MailSessionClass;

static CamelSessionClass *ms_parent_class;

static gchar *get_password(CamelSession *session, CamelService *service, const gchar *domain, const gchar *prompt, const gchar *item, guint32 flags, CamelException *ex);
static void forget_password(CamelSession *session, CamelService *service, const gchar *domain, const gchar *item, CamelException *ex);
static gboolean alert_user(CamelSession *session, CamelSessionAlertType type, const gchar *prompt, gboolean cancel);
static CamelFilterDriver *get_filter_driver(CamelSession *session, const gchar *type, CamelException *ex);
static gboolean lookup_addressbook(CamelSession *session, const gchar *name);

static void ms_thread_status(CamelSession *session, CamelSessionThreadMsg *msg, const gchar *text, gint pc);
static gpointer ms_thread_msg_new(CamelSession *session, CamelSessionThreadOps *ops, guint size);
static void ms_thread_msg_free(CamelSession *session, CamelSessionThreadMsg *m);
static void ms_forward_to (CamelSession *session, CamelFolder *folder, CamelMimeMessage *message, const gchar *address, CamelException *ex);

static void
init (MailSession *session)
{
	session->async = mail_async_event_new();
	session->junk_plugins = NULL;
}

static void
finalise (MailSession *session)
{
	if (session_check_junk_notify_id != -1)
		gconf_client_notify_remove (mail_config_get_gconf_client (), session_check_junk_notify_id);

	mail_async_event_destroy(session->async);
}

static void
class_init (MailSessionClass *mail_session_class)
{
	CamelSessionClass *camel_session_class = CAMEL_SESSION_CLASS (mail_session_class);

	/* virtual method override */
	camel_session_class->get_password = get_password;
	camel_session_class->forget_password = forget_password;
	camel_session_class->alert_user = alert_user;
	camel_session_class->get_filter_driver = get_filter_driver;
	camel_session_class->lookup_addressbook = lookup_addressbook;
	camel_session_class->thread_msg_new = ms_thread_msg_new;
	camel_session_class->thread_msg_free = ms_thread_msg_free;
	camel_session_class->thread_status = ms_thread_status;
	camel_session_class->forward_to = ms_forward_to;
}

static CamelType
mail_session_get_type (void)
{
	static CamelType mail_session_type = CAMEL_INVALID_TYPE;

	if (mail_session_type == CAMEL_INVALID_TYPE) {
		ms_parent_class = (CamelSessionClass *)camel_session_get_type();
		mail_session_type = camel_type_register (
			camel_session_get_type (),
			"MailSession",
			sizeof (MailSession),
			sizeof (MailSessionClass),
			(CamelObjectClassInitFunc) class_init,
			NULL,
			(CamelObjectInitFunc) init,
			(CamelObjectFinalizeFunc) finalise);
	}

	return mail_session_type;
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
get_password (CamelSession *session, CamelService *service, const gchar *domain,
	      const gchar *prompt, const gchar *item, guint32 flags, CamelException *ex)
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
		camel_exception_set(ex, CAMEL_EXCEPTION_USER_CANCEL, _("User canceled operation."));

	return ret;
}

static void
forget_password (CamelSession *session, CamelService *service, const gchar *domain, const gchar *item, CamelException *ex)
{
	gchar *key = make_key (service, item);

	e_passwords_forget_password (domain?domain:"Mail", key);
	g_free (key);
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

	user_message_dialog = e_error_new (NULL, error_type, m->prompt, NULL);
	g_object_set (
		user_message_dialog, "allow_shrink", TRUE,
		"allow_grow", TRUE, NULL);

	/* Use the number of dialog buttons as a heuristic for whether to
	 * emit a status bar message or present the dialog immediately, the
	 * thought being if there's more than one button then something is
	 * probably blocked until the user responds. */
	if (e_error_count_buttons (user_message_dialog) > 1) {
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
	camel_object_unref (addr);

	return ret;
}

static gboolean
alert_user(CamelSession *session, CamelSessionAlertType type, const gchar *prompt, gboolean cancel)
{
	MailSession *mail_session = MAIL_SESSION (session);
	struct _user_message_msg *m;
	gboolean result = TRUE;

	if (!mail_session->interactive)
		return FALSE;

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
	}

	if (m->ismain)
		mail_msg_unref (m);

	return result;
}

static CamelFolder *
get_folder (CamelFilterDriver *d, const gchar *uri, gpointer data, CamelException *ex)
{
	return mail_tool_uri_to_folder(uri, 0, ex);
}

static void
main_play_sound (CamelFilterDriver *driver, gchar *filename, gpointer user_data)
{
	if (filename && *filename)
		gnome_sound_play (filename);
	else
		gdk_beep ();

	g_free (filename);
	camel_object_unref (session);
}

static void
session_play_sound (CamelFilterDriver *driver, const gchar *filename, gpointer user_data)
{
	MailSession *ms = (MailSession *) session;

	camel_object_ref (session);

	mail_async_event_emit (ms->async, MAIL_ASYNC_GUI, (MailAsyncFunc) main_play_sound,
			       driver, g_strdup (filename), user_data);
}

static void
main_system_beep (CamelFilterDriver *driver, gpointer user_data)
{
	gdk_beep ();
	camel_object_unref (session);
}

static void
session_system_beep (CamelFilterDriver *driver, gpointer user_data)
{
	MailSession *ms = (MailSession *) session;

	camel_object_ref (session);

	mail_async_event_emit (ms->async, MAIL_ASYNC_GUI, (MailAsyncFunc) main_system_beep,
			       driver, user_data, NULL);
}

static CamelFilterDriver *
main_get_filter_driver (CamelSession *session, const gchar *type, CamelException *ex)
{
	CamelFilterDriver *driver;
	FilterRule *rule = NULL;
	gchar *user, *system;
	GConfClient *gconf;
	RuleContext *fc;

	gconf = mail_config_get_gconf_client ();

	user = g_strdup_printf ("%s/filters.xml", mail_component_peek_base_directory (mail_component_peek ()));
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	fc = (RuleContext *) em_filter_context_new ();
	rule_context_load (fc, system, user);
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

	if ((!strcmp (type, FILTER_SOURCE_INCOMING) || !strcmp (type, FILTER_SOURCE_JUNKTEST))
	    && camel_session_check_junk (session)) {
		/* implicit junk check as 1st rule */
		camel_filter_driver_add_rule (driver, "Junk check", "(junk-test)", "(begin (set-system-flag \"junk\"))");
	}

	if (strcmp (type, FILTER_SOURCE_JUNKTEST) != 0) {
		GString *fsearch, *faction;

		fsearch = g_string_new ("");
		faction = g_string_new ("");

		if (!strcmp (type, FILTER_SOURCE_DEMAND))
			type = FILTER_SOURCE_INCOMING;

		/* add the user-defined rules next */
		while ((rule = rule_context_next_rule (fc, rule, type))) {
			g_string_truncate (fsearch, 0);
			g_string_truncate (faction, 0);

			/* skip disabled rules */
			if (!rule->enabled)
				continue;

			filter_rule_build_code (rule, fsearch);
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
get_filter_driver (CamelSession *session, const gchar *type, CamelException *ex)
{
	return (CamelFilterDriver *) mail_call_main (MAIL_CALL_p_ppp, (MailMainFunc) main_get_filter_driver,
						     session, type, ex);
}

/* TODO: This is very temporary, until we have a better way to do the progress reporting,
   we just borrow a dummy mail-mt thread message and hook it onto out camel thread message */

static MailMsgInfo ms_thread_info_dummy = { sizeof (MailMsg) };

static gpointer ms_thread_msg_new(CamelSession *session, CamelSessionThreadOps *ops, guint size)
{
	CamelSessionThreadMsg *msg = ms_parent_class->thread_msg_new(session, ops, size);

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

static void ms_thread_msg_free(CamelSession *session, CamelSessionThreadMsg *m)
{
	mail_msg_unref(m->data);
	ms_parent_class->thread_msg_free(session, m);
}

static void ms_thread_status(CamelSession *session, CamelSessionThreadMsg *msg, const gchar *text, gint pc)
{
	/* This should never be called since we bypass it in alloc! */
	printf("Thread status '%s' %d%%\n", text, pc);
}

static void
ms_forward_to (CamelSession *session, CamelFolder *folder, CamelMimeMessage *message, const gchar *address, CamelException *ex)
{
	g_return_if_fail (session != NULL);
	g_return_if_fail (message != NULL);
	g_return_if_fail (address != NULL);

	em_utils_forward_message_raw (folder, message, address, ex);
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
		key ++;
		if (!strcmp (key, "check_incoming"))
			camel_session_set_check_junk (session, gconf_value_get_bool (gconf_entry_get_value (entry)));
	}
}

void
mail_session_init (const gchar *base_directory)
{
	gchar *camel_dir;
	GConfClient *gconf;

	if (camel_init (base_directory, TRUE) != 0)
		exit (0);

	camel_provider_init();

	session = CAMEL_SESSION (camel_object_new (MAIL_SESSION_TYPE));
	e_account_combo_box_set_session (session);  /* XXX Don't ask... */
	e_account_writable(NULL, E_ACCOUNT_SOURCE_SAVE_PASSWD); /* Init the EAccount Setup */

	camel_dir = g_strdup_printf ("%s/mail", base_directory);
	camel_session_construct (session, camel_dir);

	gconf = mail_config_get_gconf_client ();
	gconf_client_add_dir (gconf, "/apps/evolution/mail/junk", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	camel_session_set_check_junk (session, gconf_client_get_bool (gconf, "/apps/evolution/mail/junk/check_incoming", NULL));
	session_check_junk_notify_id = gconf_client_notify_add (gconf, "/apps/evolution/mail/junk",
								(GConfClientNotifyFunc) mail_session_check_junk_notify,
								session, NULL, NULL);
	session->junk_plugin = NULL;

	/* The shell will tell us to go online. */
	camel_session_set_online ((CamelSession *) session, FALSE);
	mail_config_reload_junk_headers ();
	g_free (camel_dir);
}

void
mail_session_shutdown (void)
{
	camel_shutdown ();
}

gboolean
mail_session_get_interactive (void)
{
	return MAIL_SESSION (session)->interactive;
}

void
mail_session_set_interactive (gboolean interactive)
{
	MAIL_SESSION (session)->interactive = interactive;

	if (!interactive) {
		struct _user_message_msg *msg;

		d(printf ("Gone non-interactive, checking for outstanding interactive tasks\n"));

		e_passwords_cancel();

		/* flush/cancel pending user messages */
		while (!g_queue_is_empty (&user_message_queue)) {
			msg = g_queue_pop_head (&user_message_queue);
			e_flag_set (msg->done);
			mail_msg_unref (msg);
		}

		/* and the current */
		if (user_message_dialog) {
			d(printf("Destroying message dialogue\n"));
			gtk_widget_destroy ((GtkWidget *) user_message_dialog);
		}
	}
}

void
mail_session_forget_passwords (BonoboUIComponent *uih, gpointer user_data,
			       const gchar *path)
{
	e_passwords_forget_passwords ();
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
