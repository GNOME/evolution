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

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <libgnome/gnome-config.h>
#include <libgnome/gnome-sound.h>

#include "camel/camel-filter-driver.h"
#include "filter/filter-context.h"
#include "filter/filter-filter.h"
#include "mail.h"
#include "mail-config.h"
#include "mail-session.h"
#include "mail-tools.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "e-util/e-passwords.h"
#include "e-util/e-msgport.h"

#define d(x)

CamelSession *session;


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

	/* must all be accessed with lock held ! */
	unsigned int timeout_id;/* next camel timneout id */
	EDList timeouts;	/* list of struct _timeout_data's of current or pending removed timeouts */
} MailSession;

typedef struct _MailSessionClass {
	CamelSessionClass parent_class;

} MailSessionClass;

static char *get_password(CamelSession *session, const char *prompt, gboolean reprompt, gboolean secret, CamelService *service, const char *item, CamelException *ex);
static void forget_password(CamelSession *session, CamelService *service, const char *item, CamelException *ex);
static gboolean alert_user(CamelSession *session, CamelSessionAlertType type, const char *prompt, gboolean cancel);
static guint register_timeout(CamelSession *session, guint32 interval, CamelTimeoutCallback cb, gpointer camel_data);
static gboolean remove_timeout(CamelSession *session, guint handle);
static CamelFilterDriver *get_filter_driver(CamelSession *session, const char *type, CamelException *ex);

static void
init (MailSession *session)
{
	session->lock = e_mutex_new(E_MUTEX_REC);
	session->timeout_id = 1; /* first timeout id */
	session->async = mail_async_event_new();
	e_dlist_init(&session->timeouts);
}

static void
finalise (MailSession *session)
{
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
}

static CamelType
mail_session_get_type (void)
{
	static CamelType mail_session_type = CAMEL_INVALID_TYPE;
	
	if (mail_session_type == CAMEL_INVALID_TYPE) {
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
	gboolean reprompt;
	gboolean secret;
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
	
	password_dialog = (GtkDialog *) gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_QUESTION,
								GTK_BUTTONS_OK_CANCEL, "%s", m->prompt);
	gtk_window_set_title (GTK_WINDOW (password_dialog), title);
	gtk_dialog_set_default_response (password_dialog, GTK_RESPONSE_OK);
	g_free (title);
	
	gtk_container_set_border_width ((GtkContainer *) password_dialog, 6);
	
	m->entry = gtk_entry_new ();
	gtk_entry_set_visibility ((GtkEntry *) m->entry, !m->secret);
	g_signal_connect (m->entry, "activate", G_CALLBACK (pass_activate), password_dialog);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (password_dialog)->vbox), m->entry, TRUE, FALSE, 3);
	gtk_widget_show (m->entry);
	
	if (m->reprompt && m->result) {
		gtk_entry_set_text ((GtkEntry *) m->entry, m->result);
		g_free (m->result);
		m->result = NULL;
	}
	
	if (m->service_url == NULL || m->service != NULL) {
		m->check = gtk_check_button_new_with_mnemonic (m->service_url ? _("_Remember this password") :
							       _("_Remember this password for the remainder of this session"));
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
		if (m->result == NULL || m->reprompt) {
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
get_password (CamelSession *session, const char *prompt, gboolean reprompt, gboolean secret,
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
	m->reprompt = reprompt;
	m->secret = secret;
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

static void
do_user_message (struct _mail_msg *mm)
{
	struct _user_message_msg *m = (struct _user_message_msg *)mm;
	GtkMessageType msg_type;
	
	if (!m->ismain && message_dialog != NULL) {
		e_dlist_addtail (&message_list, (EDListNode *)m);
		return;
	}
	
	switch (m->type) {
	case CAMEL_SESSION_ALERT_INFO:
		msg_type = GTK_MESSAGE_INFO;
		break;
	case CAMEL_SESSION_ALERT_WARNING:
		msg_type = GTK_MESSAGE_WARNING;
		break;
	case CAMEL_SESSION_ALERT_ERROR:
		msg_type = GTK_MESSAGE_ERROR;
		break;
	default:
		msg_type = GTK_MESSAGE_INFO;
	}
	
	message_dialog = (GtkDialog *) gtk_message_dialog_new (
		NULL, 0, msg_type,
		m->allow_cancel ? GTK_BUTTONS_OK_CANCEL : GTK_BUTTONS_OK,
		"%s", m->prompt);
	gtk_dialog_set_default_response (message_dialog, m->allow_cancel ? GTK_RESPONSE_CANCEL : GTK_RESPONSE_OK);
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

/* ******************** */

struct _timeout_data {
	struct _timeout_data *next;
	struct _timeout_data *prev;

	CamelSession *session;

	guint32 interval;

	CamelTimeoutCallback cb;
	void *camel_data;

	guint id;		/* the camel 'id' */
	guint timeout_id;	/* the gtk 'id' */

	unsigned int busy:1;		/* on if its currently running */
	unsigned int removed:1;		/* if its been removed since */
};

struct _timeout_msg {
	struct _mail_msg msg;

	CamelSession *session;
	unsigned int id;
	int result;
};

static struct _timeout_data *
find_timeout(EDList *list, unsigned int id)
{
	struct _timeout_data *td, *tn;

	td = (struct _timeout_data *)list->head;
	tn = td->next;
	while (tn) {
		if (td->id == id)
			return td;
		td = tn;
		tn = tn->next;
	}

	return NULL;
}

static void
timeout_timeout (struct _mail_msg *mm)
{
	struct _timeout_msg *m = (struct _timeout_msg *)mm;
	MailSession *ms = (MailSession *)m->session;
	struct _timeout_data *td;

	MAIL_SESSION_LOCK(ms, lock);
	td = find_timeout(&ms->timeouts, m->id);
	if (td && !td->removed) {
		if (td->busy) {
			g_warning("Timeout event dropped, still busy with last one");
		} else {
			td->busy = TRUE;
			m->result = td->cb(td->camel_data);
			td->busy = FALSE;
			td->removed = !m->result;
		}
	}
	MAIL_SESSION_UNLOCK(ms, lock);
}

static void
timeout_done (struct _mail_msg *mm)
{
	struct _timeout_msg *m = (struct _timeout_msg *) mm;
	MailSession *ms = (MailSession *) m->session;
	struct _timeout_data *td;
	
	if (!m->result) {
		MAIL_SESSION_LOCK(ms, lock);
		td = find_timeout (&ms->timeouts, m->id);
		if (td) {
			e_dlist_remove ((EDListNode *) td);
			if (td->timeout_id)
				gtk_timeout_remove (td->timeout_id);
			g_free (td);
		}
		MAIL_SESSION_UNLOCK(ms, lock);
	}
}

static void
timeout_free (struct _mail_msg *mm)
{
	struct _timeout_msg *m = (struct _timeout_msg *)mm;
	
	camel_object_unref (m->session);
}

static struct _mail_msg_op timeout_op = {
	NULL,
	timeout_timeout,
	timeout_done,
	timeout_free,
};

static gboolean 
camel_timeout (gpointer data)
{
	struct _timeout_data *td = data;
	struct _timeout_msg *m;
	
	/* stop if we are removed pending */
	if (td->removed)
		return FALSE;
	
	m = mail_msg_new (&timeout_op, NULL, sizeof (*m));
	
	m->session = td->session;
	camel_object_ref (td->session);
	m->id = td->id;
	
	e_thread_put (mail_thread_queued, (EMsg *)m);
	
	return TRUE;
}

static void
main_register_timeout (CamelSession *session, void *event_data, void *data)
{
	MailSession *ms = (MailSession *)session;
	unsigned int handle = GPOINTER_TO_UINT(event_data);
	struct _timeout_data *td;
	
	MAIL_SESSION_LOCK(session, lock);
	td = find_timeout (&ms->timeouts, handle);
	if (td) {
		if (td->removed) {
			e_dlist_remove ((EDListNode *) td);
			if (td->timeout_id)
				gtk_timeout_remove (td->timeout_id);
			g_free (td);
		} else {
			td->timeout_id = gtk_timeout_add (td->interval, camel_timeout, td);
		}
	}
	MAIL_SESSION_UNLOCK(session, lock);
	
	camel_object_unref (ms);
}

static guint
register_timeout (CamelSession *session, guint32 interval, CamelTimeoutCallback cb, gpointer camel_data)
{
	struct _timeout_data *td;
	MailSession *ms = (MailSession *) session;
	guint ret;
	
	MAIL_SESSION_LOCK(session, lock);
	
	ret = ms->timeout_id;
	ms->timeout_id++;
	
	/* just debugging, the timeout code now ignores excessive events anyway */
	if (interval < 100)
		g_warning ("Timeout requested %d is small, may cause performance problems", interval);
	
	td = g_malloc (sizeof (*td));
	td->cb = cb;
	td->camel_data = camel_data;
	td->interval = interval;
	td->id = ret;
	td->session = session;
	td->removed = FALSE;
	td->busy = FALSE;
	e_dlist_addhead (&ms->timeouts, (EDListNode *) td);
	
	MAIL_SESSION_UNLOCK(session, lock);
	
	camel_object_ref (ms);
	mail_async_event_emit (ms->async, MAIL_ASYNC_GUI, (MailAsyncFunc) main_register_timeout,
			       (CamelObject *) session, GUINT_TO_POINTER(ret), NULL);

	return ret;
}

static void
main_remove_timeout (CamelSession *session, void *event_data, void *data)
{
	MailSession *ms = (MailSession *) session;
	unsigned int handle = GPOINTER_TO_UINT(event_data);
	struct _timeout_data *td;
	
	MAIL_SESSION_LOCK(session, lock);
	td = find_timeout (&ms->timeouts, handle);
	if (td) {
		e_dlist_remove ((EDListNode *) td);
		if (td->timeout_id)
			gtk_timeout_remove (td->timeout_id);
		g_free (td);
	}
	MAIL_SESSION_UNLOCK(session, lock);
	
	camel_object_unref (ms);
}

static gboolean
remove_timeout (CamelSession *session, guint handle)
{
	MailSession *ms = (MailSession *)session;
	struct _timeout_data *td;
	int remove = FALSE;
	
	MAIL_SESSION_LOCK(session, lock);
	td = find_timeout (&ms->timeouts, handle);
	if (td && !td->removed) {
		td->removed = TRUE;
		remove = TRUE;
	}
	MAIL_SESSION_UNLOCK(session, lock);
	
	if (remove) {
		camel_object_ref (ms);
		mail_async_event_emit (ms->async, MAIL_ASYNC_GUI, (MailAsyncFunc) main_remove_timeout,
				       (CamelObject *) session, GUINT_TO_POINTER(handle), NULL);
	} else
		g_warning ("Removing a timeout i dont know about (or twice): %d", handle);

	return TRUE;
}

static CamelFolder *
get_folder (CamelFilterDriver *d, const char *uri, void *data, CamelException *ex)
{
	return mail_tool_uri_to_folder (uri, 0, ex);
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
	GString *fsearch, *faction;
	FilterRule *rule = NULL;
	char *user, *system;
	GConfClient *gconf;
	RuleContext *fc;
	long notify;
	
	gconf = mail_config_get_gconf_client ();
	
	user = g_strdup_printf ("%s/filters.xml", evolution_dir);
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
	
	fsearch = g_string_new ("");
	faction = g_string_new ("");
	
	/* add the new-mail notification rule first to be sure that it gets invoked */
	
	/* FIXME: we need a way to distinguish between filtering new
           mail and re-filtering a folder because both use the
           "incoming" filter type */
	notify = gconf_client_get_int (gconf, "/apps/evolution/mail/notify/type", NULL);
	if (notify != MAIL_CONFIG_NOTIFY_NOT && !strcmp (type, "incoming")) {
		char *filename;
		
		g_string_truncate (faction, 0);
		
		g_string_append (faction, "(only-once \"new-mail-notification\" ");
		
		switch (notify) {
		case MAIL_CONFIG_NOTIFY_PLAY_SOUND:
			filename = gconf_client_get_string (gconf, "/apps/evolution/mail/notify/sound", NULL);
			if (filename) {
				g_string_append_printf (faction, "\"(play-sound \\\"%s\\\")\"", filename);
				g_free (filename);
				break;
			}
			/* fall through */
		case MAIL_CONFIG_NOTIFY_BEEP:
			g_string_append (faction, "\"(beep)\"");
			break;
		default:
			break;
		}
		
		g_string_append (faction, ")");
		
		camel_filter_driver_add_rule (driver, "new-mail-notification", "(begin #t)", faction->str);
	}
	
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
	
	g_object_unref (fc);
	
	return driver;
}

static CamelFilterDriver *
get_filter_driver (CamelSession *session, const char *type, CamelException *ex)
{
	return (CamelFilterDriver *) mail_call_main (MAIL_CALL_p_ppp, (MailMainFunc) main_get_filter_driver,
						     session, type, ex);
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

void
mail_session_init (void)
{
	char *camel_dir;
	
	if (camel_init (evolution_dir, TRUE) != 0)
		exit (0);
	
	session = CAMEL_SESSION (camel_object_new (MAIL_SESSION_TYPE));
	
	camel_dir = g_strdup_printf ("%s/mail", evolution_dir);
	camel_session_construct (session, camel_dir);
	
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
