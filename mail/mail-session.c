/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-session.c: handles the session information and resource manipulation */
/*
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkcheckbutton.h>

#include <gconf/gconf-client.h>

#include <libgnome/gnome-config.h>
#include <libgnome/gnome-sound.h>

#include <camel/camel.h>	/* FIXME: this is where camel_init is defined, it shouldn't include everything else */
#include "camel/camel-filter-driver.h"

#include "filter/filter-context.h"
#include "filter/filter-filter.h"
#include "mail-component.h"
#include "mail-config.h"
#include "mail-session.h"
#include "mail-tools.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "e-util/e-passwords.h"
#include "e-util/e-msgport.h"
#include "em-junk-filter.h"
#include "widgets/misc/e-error.h"

#define d(x)

CamelSession *session;
static int session_check_junk_notify_id = -1;

#define MAIL_SESSION_TYPE     (mail_session_get_type ())
#define MAIL_SESSION(obj)     (CAMEL_CHECK_CAST((obj), MAIL_SESSION_TYPE, MailSession))
#define MAIL_SESSION_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), MAIL_SESSION_TYPE, MailSessionClass))
#define MAIL_IS_SESSION(o)    (CAMEL_CHECK_TYPE((o), MAIL_SESSION_TYPE))

#define MAIL_SESSION_LOCK(s, l) (e_mutex_lock(((MailSession *)s)->l))
#define MAIL_SESSION_UNLOCK(s, l) (e_mutex_unlock(((MailSession *)s)->l))

typedef struct _MailSession {
	CamelSession parent_object;

	gboolean interactive;
	FILE *filter_logfile;

	EMutex *lock;

	MailAsyncEvent *async;
} MailSession;

typedef struct _MailSessionClass {
	CamelSessionClass parent_class;

} MailSessionClass;

static CamelSessionClass *ms_parent_class;

static char *get_password(CamelSession *session, const char *prompt, guint32 flags, CamelService *service, const char *item, CamelException *ex);
static void forget_password(CamelSession *session, CamelService *service, const char *item, CamelException *ex);
static gboolean alert_user(CamelSession *session, CamelSessionAlertType type, const char *prompt, gboolean cancel);
static CamelFilterDriver *get_filter_driver(CamelSession *session, const char *type, CamelException *ex);

static void ms_thread_status(CamelSession *session, CamelSessionThreadMsg *msg, const char *text, int pc);
static void *ms_thread_msg_new(CamelSession *session, CamelSessionThreadOps *ops, unsigned int size);
static void ms_thread_msg_free(CamelSession *session, CamelSessionThreadMsg *m);

static void
init (MailSession *session)
{
	session->lock = e_mutex_new(E_MUTEX_REC);
	session->async = mail_async_event_new();
}

static void
finalise (MailSession *session)
{
	if (session_check_junk_notify_id != -1)
		gconf_client_notify_remove (mail_config_get_gconf_client (), session_check_junk_notify_id);

	mail_async_event_destroy(session->async);
	e_mutex_destroy(session->lock);
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

	camel_session_class->thread_msg_new = ms_thread_msg_new;
	camel_session_class->thread_msg_free = ms_thread_msg_free;
	camel_session_class->thread_status = ms_thread_status;
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


static char *
make_key (CamelService *service, const char *item)
{
	char *key;
	
	if (service)
		key = camel_url_to_string (service->url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	else
		key = g_strdup (item);
	
	return key;
}

/* ********************************************************************** */

static GtkDialog *password_dialog = NULL;
static EDList password_list = E_DLIST_INITIALISER(password_list);

struct _pass_msg {
	struct _mail_msg msg;
	
	CamelSession *session;
	const char *prompt;
	guint32 flags;
	CamelService *service;
	const char *item;
	CamelException *ex;
	
	char *service_url;
	char *key;
	
	EAccountService *config_service;
	GtkWidget *check;
	GtkWidget *entry;
	char *result;
	int ismain;
};

static void do_get_pass(struct _mail_msg *mm);

static void
pass_activate (GtkEntry *entry, void *data)
{
	if (password_dialog)
		gtk_dialog_response (password_dialog, GTK_RESPONSE_OK);
}

static void
pass_response (GtkDialog *dialog, int button, void *data)
{
	struct _pass_msg *m = data;
	
	switch (button) {
	case GTK_RESPONSE_OK:
	{
		gboolean cache, remember;
		
		m->result = g_strdup (gtk_entry_get_text ((GtkEntry *) m->entry));
		remember = cache = m->check ? gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (m->check)) : FALSE;
		
		if (m->service_url) {
			if (m->config_service) {
				mail_config_service_set_save_passwd (m->config_service, cache);
				
				/* set `cache' to TRUE because people don't want to have to
				   re-enter their passwords for this session even if they told
				   us not to cache their passwords in the dialog...*sigh* */
				cache = TRUE;
			}
		} else {
			/* we can't remember the password if it isn't for an account (pgp?) */
			remember = FALSE;
		}
		
		if (cache) {
			/* cache the password for the session */
			e_passwords_add_password (m->key, m->result);
			
			/* should we remember it between sessions? */
			if (remember)
				e_passwords_remember_password ("Mail", m->key);
		}
		break;
	}
	default:
		camel_exception_set (m->ex, CAMEL_EXCEPTION_USER_CANCEL, _("User canceled operation."));
		break;
	}

	gtk_widget_destroy ((GtkWidget *) dialog);

	password_dialog = NULL;
	e_msgport_reply ((EMsg *)m);
	
	if ((m = (struct _pass_msg *) e_dlist_remhead (&password_list)))
		do_get_pass ((struct _mail_msg *) m);
}

static void
request_password (struct _pass_msg *m)
{
	EAccount *mca = NULL;
	char *title;
	
	/* If we already have a password_dialog up, save this request till later */
	if (!m->ismain && password_dialog) {
		e_dlist_addtail (&password_list, (EDListNode *)m);
		return;
	}
	
	if (m->service_url) {
		if ((mca = mail_config_get_account_by_source_url (m->service_url)))
			m->config_service = mca->source;
		else if ((mca = mail_config_get_account_by_transport_url (m->service_url)))
			m->config_service = mca->transport;
	}
	
	if (mca)
		title = g_strdup_printf (_("Enter Password for %s"), mca->name);
	else
		title = g_strdup (_("Enter Password"));
	
	password_dialog = (GtkDialog *)e_error_new(NULL, "mail:ask-session-password", m->prompt, NULL);
	gtk_window_set_title (GTK_WINDOW (password_dialog), title);
	g_free (title);
	
	m->entry = gtk_entry_new ();
	gtk_entry_set_visibility ((GtkEntry *) m->entry, !(m->flags & CAMEL_SESSION_PASSWORD_SECRET));
	g_signal_connect (m->entry, "activate", G_CALLBACK (pass_activate), password_dialog);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (password_dialog)->vbox), m->entry, TRUE, FALSE, 3);
	gtk_widget_show (m->entry);
	gtk_widget_grab_focus (m->entry);
	
	if ((m->flags & CAMEL_SESSION_PASSWORD_REPROMPT) && m->result) {
		gtk_entry_set_text ((GtkEntry *) m->entry, m->result);
		g_free (m->result);
		m->result = NULL;
	}

	/* static password, shouldn't be remembered between sessions,
	   but will be remembered within the session beyond our control */
	if ((m->service_url == NULL || m->service != NULL)
	    && (m->flags & CAMEL_SESSION_PASSWORD_STATIC) == 0) {
		m->check = gtk_check_button_new_with_mnemonic (m->service_url ? _("_Remember this password")
							       : _("_Remember this password for the remainder of this session"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (m->check),
					      m->config_service ? m->config_service->save_passwd : FALSE);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (password_dialog)->vbox), m->check, TRUE, FALSE, 3);
		gtk_widget_show (m->check);
	}
	
	if (m->ismain) {
		pass_response(password_dialog, gtk_dialog_run (password_dialog), m);
	} else {
		g_signal_connect (password_dialog, "response", G_CALLBACK (pass_response), m);
		gtk_widget_show ((GtkWidget *) password_dialog);
	}
}

static void
do_get_pass(struct _mail_msg *mm)
{
	struct _pass_msg *m = (struct _pass_msg *)mm;
	MailSession *mail_session = MAIL_SESSION (m->session);
	
	if (!strcmp (m->item, "popb4smtp_uri")) {
		char *url = camel_url_to_string (m->service->url, 0);
		EAccount *account = mail_config_get_account_by_transport_url (url);
		
		g_free(url);
		if (account)
			m->result = g_strdup(account->source->url);
	} else if (m->key) {
		m->result = e_passwords_get_password ("Mail", m->key);
		if (m->result == NULL || (m->flags & CAMEL_SESSION_PASSWORD_REPROMPT)) {
			if (mail_session->interactive) {
				request_password(m);
				return;
			}
		}
	}

	e_msgport_reply((EMsg *)mm);
}

static void
do_free_pass(struct _mail_msg *mm)
{
	struct _pass_msg *m = (struct _pass_msg *)mm;

	g_free(m->service_url);
	g_free(m->key);
}

static struct _mail_msg_op get_pass_op = {
	NULL,
	do_get_pass,
	NULL,
	do_free_pass,
};

static char *
get_password (CamelSession *session, const char *prompt, guint32 flags,
	      CamelService *service, const char *item, CamelException *ex)
{
	struct _pass_msg *m, *r;
	EMsgPort *pass_reply;
	char *ret;
	
	/* We setup an async request and send it off, and wait for it to return */
	/* If we're really in main, we dont of course ...
	   ... but this shouldn't be allowed because of locking issues */
	pass_reply = e_msgport_new ();
	m = mail_msg_new(&get_pass_op, pass_reply, sizeof(struct _pass_msg));
	m->ismain = pthread_self() == mail_gui_thread;
	m->session = session;
	m->prompt = prompt;
	m->flags = flags;
	m->service = service;
	m->item = item;
	m->ex = ex;
	if (service)
		m->service_url = camel_url_to_string (service->url, CAMEL_URL_HIDE_ALL);
	m->key = make_key(service, item);

	if (m->ismain) {
		do_get_pass((struct _mail_msg *)m);
	} else {
		extern EMsgPort *mail_gui_port2;
		
		e_msgport_put(mail_gui_port2, (EMsg *)m);
	}
	
	e_msgport_wait(pass_reply);
	r = (struct _pass_msg *)e_msgport_get(pass_reply);
	g_assert(m == r);

	ret = m->result;
	mail_msg_free(m);
	e_msgport_destroy(pass_reply);
	
	return ret;
}

static void
main_forget_password (CamelSession *session, CamelService *service, const char *item, CamelException *ex)
{
	char *key = make_key (service, item);
	
	e_passwords_forget_password ("Mail", key);
	
	g_free (key);
}

static void
forget_password (CamelSession *session, CamelService *service, const char *item, CamelException *ex)
{
	mail_call_main(MAIL_CALL_p_pppp, (MailMainFunc)main_forget_password,
		       session, service, item, ex);
}

/* ********************************************************************** */

static GtkDialog *message_dialog;
static EDList message_list = E_DLIST_INITIALISER(message_list);

struct _user_message_msg {
	struct _mail_msg msg;

	CamelSessionAlertType type;
	const char *prompt;

	unsigned int allow_cancel:1;
	unsigned int result:1;
	unsigned int ismain:1;
};

static void do_user_message (struct _mail_msg *mm);

/* clicked, send back the reply */
static void
user_message_response (GtkDialog *dialog, int button, struct _user_message_msg *m)
{
	gtk_widget_destroy ((GtkWidget *) dialog);
	
	message_dialog = NULL;
	
	/* if !allow_cancel, then we've already replied */
	if (m->allow_cancel) {
		m->result = button == GTK_RESPONSE_OK;
		e_msgport_reply((EMsg *)m);
	}
	
	/* check for pendings */
	if ((m = (struct _user_message_msg *)e_dlist_remhead(&message_list)))
		do_user_message((struct _mail_msg *)m);
}

static void
user_message_destroy_notify (struct _user_message_msg *m, GObject *deadbeef)
{
	message_dialog = NULL;
}

/* This is kinda ugly/inefficient, but oh well, it works */
static const char *error_type[] = {
	"mail:session-message-info", "mail:session-message-warning", "mail:session-message-error",
	"mail:session-message-info-cancel", "mail:session-message-warning-cancel", "mail:session-message-error-cancel"
};

static void
do_user_message (struct _mail_msg *mm)
{
	struct _user_message_msg *m = (struct _user_message_msg *)mm;
	int type;
	
	if (!m->ismain && message_dialog != NULL) {
		e_dlist_addtail (&message_list, (EDListNode *)m);
		return;
	}
	
	switch (m->type) {
	case CAMEL_SESSION_ALERT_INFO:
		type = 0;
		break;
	case CAMEL_SESSION_ALERT_WARNING:
		type = 1;
		break;
	case CAMEL_SESSION_ALERT_ERROR:
		type = 2;
		break;
	default:
		type = 0;
	}

	if (m->allow_cancel)
		type += 3;
	
	message_dialog = (GtkDialog *)e_error_new(NULL, error_type[type], m->prompt, NULL);
	g_object_set ((GObject *) message_dialog, "allow_shrink", TRUE, "allow_grow", TRUE, NULL);
	
	/* We only need to wait for the result if we allow cancel otherwise show but send result back instantly */
	if (m->allow_cancel) {
		if (m->ismain) {
			user_message_response(message_dialog, gtk_dialog_run (message_dialog), m);
		} else {
			g_signal_connect (message_dialog, "response", G_CALLBACK (user_message_response), m);
			gtk_widget_show ((GtkWidget *) message_dialog);
		}
	} else {
		g_signal_connect (message_dialog, "response", G_CALLBACK (gtk_widget_destroy), message_dialog);
		g_object_weak_ref ((GObject *) message_dialog, (GWeakNotify) user_message_destroy_notify, m);
		gtk_widget_show ((GtkWidget *) message_dialog);
		m->result = TRUE;
		e_msgport_reply ((EMsg *)m);
	}
}

static struct _mail_msg_op user_message_op = { NULL, do_user_message, NULL, NULL };

static gboolean
alert_user(CamelSession *session, CamelSessionAlertType type, const char *prompt, gboolean cancel)
{
	MailSession *mail_session = MAIL_SESSION (session);
	struct _user_message_msg *m, *r;
	EMsgPort *user_message_reply;
	gboolean ret;

	if (!mail_session->interactive)
		return FALSE;

	user_message_reply = e_msgport_new ();	
	m = mail_msg_new (&user_message_op, user_message_reply, sizeof (*m));
	m->ismain = pthread_self() == mail_gui_thread;
	m->type = type;
	m->prompt = prompt;
	m->allow_cancel = cancel;

	if (m->ismain)
		do_user_message((struct _mail_msg *)m);
	else {
		extern EMsgPort *mail_gui_port2;

		e_msgport_put(mail_gui_port2, (EMsg *)m);
	}

	e_msgport_wait(user_message_reply);
	r = (struct _user_message_msg *)e_msgport_get(user_message_reply);
	g_assert(m == r);

	ret = m->result;
	mail_msg_free(m);
	e_msgport_destroy(user_message_reply);

	return ret;
}

static CamelFolder *
get_folder (CamelFilterDriver *d, const char *uri, void *data, CamelException *ex)
{
	return mail_tool_uri_to_folder(uri, 0, ex);
}

static void
main_play_sound (CamelFilterDriver *driver, char *filename, gpointer user_data)
{
	if (filename && *filename)
		gnome_sound_play (filename);
	else
		gdk_beep ();
	
	g_free (filename);
	camel_object_unref (session);
}

static void
session_play_sound (CamelFilterDriver *driver, const char *filename, gpointer user_data)
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
main_get_filter_driver (CamelSession *session, const char *type, CamelException *ex)
{
	CamelFilterDriver *driver;
	FilterRule *rule = NULL;
	char *user, *system;
	GConfClient *gconf;
	RuleContext *fc;
	
	gconf = mail_config_get_gconf_client ();
	
	user = g_strdup_printf ("%s/mail/filters.xml", mail_component_peek_base_directory (mail_component_peek ()));
	system = EVOLUTION_PRIVDATADIR "/filtertypes.xml";
	fc = (RuleContext *) filter_context_new ();
	rule_context_load (fc, system, user);
	g_free (user);
	
	driver = camel_filter_driver_new (session);
	camel_filter_driver_set_folder_func (driver, get_folder, NULL);
	
	if (gconf_client_get_bool (gconf, "/apps/evolution/mail/filters/log", NULL)) {
		MailSession *ms = (MailSession *) session;
		
		if (ms->filter_logfile == NULL) {
			char *filename;
			
			filename = gconf_client_get_string (gconf, "/apps/evolution/mail/filters/logfile", NULL);
			if (filename) {
				ms->filter_logfile = fopen (filename, "a+");
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
		camel_filter_driver_add_rule (driver, "Junk check", "(junk-test)", "(begin (set-system-flag \"junk\")(set-system-flag \"seen\"))");
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
		
			filter_rule_build_code (rule, fsearch);
			filter_filter_build_action ((FilterFilter *) rule, faction);
			camel_filter_driver_add_rule (driver, rule->name, fsearch->str, faction->str);
		}
		
		g_string_free (fsearch, TRUE);
		g_string_free (faction, TRUE);
	}
	
	g_object_unref (fc);
	
	return driver;
}

static CamelFilterDriver *
get_filter_driver (CamelSession *session, const char *type, CamelException *ex)
{
	return (CamelFilterDriver *) mail_call_main (MAIL_CALL_p_ppp, (MailMainFunc) main_get_filter_driver,
						     session, type, ex);
}

/* TODO: This is very temporary, until we have a better way to do the progress reporting,
   we just borrow a dummy mail-mt thread message and hook it onto out camel thread message */

static mail_msg_op_t ms_thread_ops_dummy = { NULL };

static void *ms_thread_msg_new(CamelSession *session, CamelSessionThreadOps *ops, unsigned int size)
{
	CamelSessionThreadMsg *msg = ms_parent_class->thread_msg_new(session, ops, size);

	/* We create a dummy mail_msg, and then copy its cancellation port over to ours, so
	   we get cancellation and progress in common with hte existing mail code, for free */
	if (msg) {
		struct _mail_msg *m = mail_msg_new(&ms_thread_ops_dummy, NULL, sizeof(struct _mail_msg));

		msg->data = m;
		camel_operation_unref(msg->op);
		msg->op = m->cancel;
		camel_operation_ref(msg->op);
	}

	return msg;
}

static void ms_thread_msg_free(CamelSession *session, CamelSessionThreadMsg *m)
{
	mail_msg_free(m->data);
	ms_parent_class->thread_msg_free(session, m);
}

static void ms_thread_status(CamelSession *session, CamelSessionThreadMsg *msg, const char *text, int pc)
{
	/* This should never be called since we bypass it in alloc! */
	printf("Thread status '%s' %d%%\n", text, pc);
}

char *
mail_session_get_password (const char *url_string)
{
	CamelURL *url;
	char *simple_url;
	char *passwd;
	
	url = camel_url_new (url_string, NULL);
	simple_url = camel_url_to_string (url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	camel_url_free (url);
	
	passwd = e_passwords_get_password ("Mail", simple_url);
	
	g_free (simple_url);
	
	return passwd;
}

void
mail_session_add_password (const char *url_string,
			   const char *passwd)
{
	CamelURL *url;
	char *simple_url;
	
	url = camel_url_new (url_string, NULL);
	simple_url = camel_url_to_string (url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	camel_url_free (url);
	
	e_passwords_add_password (simple_url, passwd);
	
	g_free (simple_url);
}

void
mail_session_remember_password (const char *url_string)
{
	CamelURL *url;
	char *simple_url;
	
	url = camel_url_new (url_string, NULL);
	simple_url = camel_url_to_string (url, CAMEL_URL_HIDE_PASSWORD | CAMEL_URL_HIDE_PARAMS);
	camel_url_free (url);
	
	e_passwords_remember_password ("Mail", simple_url);
	
	g_free (simple_url);
}

void
mail_session_forget_password (const char *key)
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
mail_session_init (const char *base_directory)
{
	char *camel_dir;
	GConfClient *gconf;
	
	if (camel_init (base_directory, TRUE) != 0)
		exit (0);
	
	session = CAMEL_SESSION (camel_object_new (MAIL_SESSION_TYPE));
	
	camel_dir = g_strdup_printf ("%s/mail", base_directory);
	camel_session_construct (session, camel_dir);

	gconf = mail_config_get_gconf_client ();
	gconf_client_add_dir (gconf, "/apps/evolution/mail/junk", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	camel_session_set_check_junk (session, gconf_client_get_bool (gconf, "/apps/evolution/mail/junk/check_incoming", NULL));
	session_check_junk_notify_id = gconf_client_notify_add (gconf, "/apps/evolution/mail/junk",
								(GConfClientNotifyFunc) mail_session_check_junk_notify,
								session, NULL, NULL);
	session->junk_plugin = CAMEL_JUNK_PLUGIN (em_junk_filter_get_plugin ());

	/* The shell will tell us to go online. */
	camel_session_set_online ((CamelSession *) session, FALSE);
	
	g_free (camel_dir);
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
		struct _pass_msg *pm;
		struct _user_message_msg *um;
		
		d(printf ("Gone non-interactive, checking for outstanding interactive tasks\n"));
		
		/* clear out pending password requests */
		while ((pm = (struct _pass_msg *) e_dlist_remhead (&password_list))) {
			d(printf ("Flushing password request : %s\n", pm->prompt));
			e_msgport_reply ((EMsg *) pm);
		}
		
		/* destroy the current */
		if (password_dialog) {
			d(printf ("Destroying password dialogue\n"));
			gtk_widget_destroy ((GtkWidget *) password_dialog);
			password_dialog =  NULL;
		}
		
		/* same for pending user messages */
		while ((um = (struct _user_message_msg *) e_dlist_remhead (&message_list))) {
			d(printf ("Flusing message request: %s\n", um->prompt));
			e_msgport_reply((EMsg *) um);
		}
		
		/* and the current */
		if (message_dialog) {
			d(printf("Destroying message dialogue\n"));
			gtk_widget_destroy ((GtkWidget *) message_dialog);
		}
	}
}

void
mail_session_forget_passwords (BonoboUIComponent *uih, void *user_data,
			       const char *path)
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
