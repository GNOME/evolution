/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-session.c: handles the session information and resource manipulation */
/*
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#include <stdlib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>
#include "camel/camel-filter-driver.h"
#include "filter/filter-context.h"
#include "filter/filter-filter.h"
#include "mail.h"
#include "mail-session.h"
#include "mail-tools.h"
#include "mail-mt.h"
#include "e-util/e-passwords.h"

#define d(x)

CamelSession *session;


#define MAIL_SESSION_TYPE     (mail_session_get_type ())
#define MAIL_SESSION(obj)     (CAMEL_CHECK_CAST((obj), MAIL_SESSION_TYPE, MailSession))
#define MAIL_SESSION_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), MAIL_SESSION_TYPE, MailSessionClass))
#define MAIL_IS_SESSION(o)    (CAMEL_CHECK_TYPE((o), MAIL_SESSION_TYPE))


typedef struct _MailSession {
	CamelSession parent_object;

	gboolean interaction_enabled;
	FILE *filter_logfile;
} MailSession;

typedef struct _MailSessionClass {
	CamelSessionClass parent_class;

} MailSessionClass;


static char *get_password (CamelSession *session, const char *prompt,
			   gboolean secret, CamelService *service,
			   const char *item, CamelException *ex);
static void forget_password (CamelSession *session, CamelService *service,
			     const char *item, CamelException *ex);
static gboolean alert_user (CamelSession *session, CamelSessionAlertType type,
			    const char *prompt, gboolean cancel);
static guint register_timeout (CamelSession *session, guint32 interval,
			       CamelTimeoutCallback cb, gpointer camel_data);
static gboolean remove_timeout (CamelSession *session, guint handle);
static CamelFilterDriver *get_filter_driver (CamelSession *session,
					     const char *type,
					     CamelException *ex);


static void
init (MailSession *session)
{
}

static void
class_init (MailSessionClass *mail_session_class)
{
	CamelSessionClass *camel_session_class =
		CAMEL_SESSION_CLASS (mail_session_class);
	
	/* virtual method override */
	camel_session_class->get_password = get_password;
	camel_session_class->forget_password = forget_password;
	camel_session_class->alert_user = alert_user;
	camel_session_class->register_timeout = register_timeout;
	camel_session_class->remove_timeout = remove_timeout;
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
			NULL);
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

static GnomeDialog *password_dialogue = NULL;
static EDList password_list = E_DLIST_INITIALISER(password_list);
static struct _pass_msg *password_current = NULL;
static int password_destroy_id;

struct _pass_msg {
	struct _mail_msg msg;

	CamelSession *session;
	const char *prompt;
	gboolean secret;
	CamelService *service;
	const char *item;
	CamelException *ex;

	char *service_url;
	char *key;

	GtkWidget *check;
	char *result;
	int ismain;
};

static void do_get_pass(struct _mail_msg *mm);

static void
pass_got (char *string, void *data)
{
	struct _pass_msg *m = data;

	if (string) {
		MailConfigService *service = NULL;
		const MailConfigAccount *mca;
		gboolean remember;
		
		m->result = g_strdup (string);
		remember = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (m->check));
		if (m->service_url) {
			mca = mail_config_get_account_by_source_url (m->service_url);
			if (mca) {
				service = mca->source;
			} else {
				mca = mail_config_get_account_by_transport_url (m->service_url);
				if (mca)
					service = mca->transport;
			}
			
			if (service) {
				mail_config_service_set_save_passwd (service, remember);
				
				/* set `remember' to TRUE because people don't want to have to
				   re-enter their passwords for this session even if they told
				   us not to cache their passwords in the dialog...*sigh* */
				remember = TRUE;
			}
		}
		
		if (remember)
			e_passwords_add_password(m->key, m->result);
	} else {
		camel_exception_set(m->ex, CAMEL_EXCEPTION_USER_CANCEL, _("User canceled operation."));
	}

	if (password_destroy_id) {
		gtk_signal_disconnect((GtkObject *)password_dialogue, password_destroy_id);
		password_destroy_id = 0;
	}

	password_dialogue = NULL;
	e_msgport_reply((EMsg *)m);

	if ((m = (struct _pass_msg *)e_dlist_remhead(&password_list)))
		do_get_pass((struct _mail_msg *)m);
}

static void
request_password_deleted(GtkWidget *w, struct _pass_msg *m)
{
	password_destroy_id = 0;
	pass_got(NULL, m);
}

static void
request_password(struct _pass_msg *m)
{
	const MailConfigAccount *mca = NULL;
	GtkWidget *dialogue;
	GtkWidget *check, *entry;
	GList *children, *iter;
	gboolean show;
	char *title;

	/* If we already have a password_dialogue up, save this request till later */
	if (!m->ismain && password_dialogue) {
		e_dlist_addtail(&password_list, (EDListNode *)m);
		return;
	}

	password_current = m;

	/* FIXME: Remove this total snot */

	/* this api is just awful ... hence the major hacks */
	password_dialogue = dialogue = gnome_request_dialog (m->secret, m->prompt, NULL, 0, pass_got, m, NULL);

	/* cant bleieve how @!@#!@# 5this api is, it doesn't handle this for you, BLAH! */
	password_destroy_id = gtk_signal_connect((GtkObject *)dialogue, "destroy", request_password_deleted, m);

	/* Remember the password? */
	check = gtk_check_button_new_with_label (m->service_url ? _("Remember this password") :
						 _("Remember this password for the remainder of this session"));
	show = TRUE;
	
	if (m->service_url) {
		mca = mail_config_get_account_by_source_url(m->service_url);
		if (mca)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), mca->source->save_passwd);
		else {
			mca = mail_config_get_account_by_transport_url (m->service_url);
			if (mca)
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), mca->transport->save_passwd);
			else {
				d(printf ("Cannot figure out which account owns URL \"%s\"\n", m->service_url));
				show = FALSE;
			}
		}
	}
	
	if (show)
		gtk_widget_show (check);
	
	/* do some dirty stuff to put the checkbutton after the entry */
	entry = NULL;
	children = gtk_container_children (GTK_CONTAINER (GNOME_DIALOG (dialogue)->vbox));
	for (iter = children; iter; iter = iter->next) {
		if (GTK_IS_ENTRY (iter->data)) {
			entry = GTK_WIDGET (iter->data);
			break;
		}
	}
	g_list_free (children);
	
	if (entry) {
		gtk_object_ref (GTK_OBJECT (entry));
		gtk_container_remove (GTK_CONTAINER (GNOME_DIALOG (dialogue)->vbox), entry);
	}
	
	gtk_box_pack_end (GTK_BOX (GNOME_DIALOG (dialogue)->vbox), check, TRUE, FALSE, 0);
	
	if (entry) {
		gtk_box_pack_end (GTK_BOX (GNOME_DIALOG (dialogue)->vbox), entry, TRUE, FALSE, 0);
		gtk_widget_grab_focus (entry);
		gtk_object_unref (GTK_OBJECT (entry));
	}
	
	m->check = check;
	
	if (mca) {
		char *name;
		
		name = e_utf8_to_gtk_string (GTK_WIDGET (dialogue), mca->name);
		title = g_strdup_printf (_("Enter Password for %s"), name);
		g_free (name);
	} else
		title = g_strdup (_("Enter Password"));
	
	gtk_window_set_title (GTK_WINDOW (dialogue), title);
	g_free (title);

	if (m->ismain) {
		password_current = NULL;
		gnome_dialog_run_and_close ((GnomeDialog *)dialogue);
	} else {
		gtk_widget_show(dialogue);
	}
}

static void
do_get_pass(struct _mail_msg *mm)
{
	struct _pass_msg *m = (struct _pass_msg *)mm;
	MailSession *mail_session = MAIL_SESSION (m->session);
	
	if (!strcmp (m->item, "popb4smtp_uri")) {
		char *url = camel_url_to_string(m->service->url, 0);
		const MailConfigAccount *account = mail_config_get_account_by_transport_url(url);
		
		g_free(url);
		if (account)
			m->result = g_strdup(account->source->url);
	} else if (m->key) {
		m->result = e_passwords_get_password(m->key);
		if (m->result == NULL) {
			if (mail_session->interaction_enabled) {
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
get_password (CamelSession *session, const char *prompt, gboolean secret, CamelService *service, const char *item, CamelException *ex)
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
	m->secret = secret;
	m->service = service;
	m->item = item;
	m->ex = ex;
	if (service)
		m->service_url = camel_url_to_string (service->url, CAMEL_URL_HIDE_ALL);
	m->key = make_key(service, item);

	if (m->ismain)
		do_get_pass(m);
	else {
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

	e_passwords_forget_password (key);
	
	g_free (key);
}

static void
forget_password (CamelSession *session, CamelService *service, const char *item, CamelException *ex)
{
	mail_call_main(MAIL_CALL_p_pppp, (MailMainFunc)main_forget_password,
		       session, service, item, ex);
}

static gboolean
alert_user (CamelSession *session, CamelSessionAlertType type,
	    const char *prompt, gboolean cancel)
{
	MailSession *mail_session = MAIL_SESSION (session);
	const char *message_type = NULL;
	
	if (!mail_session->interaction_enabled)
		return FALSE;
	
	switch (type) {
	case CAMEL_SESSION_ALERT_INFO:
		message_type = GNOME_MESSAGE_BOX_INFO;
		break;
	case CAMEL_SESSION_ALERT_WARNING:
		message_type = GNOME_MESSAGE_BOX_WARNING;
		break;
	case CAMEL_SESSION_ALERT_ERROR:
		message_type = GNOME_MESSAGE_BOX_ERROR;
		break;
	}
	return mail_user_message (message_type, prompt, cancel);
}

/* ******************** */

struct _timeout_data {
	CamelTimeoutCallback cb;
	guint32 interval;
	void *camel_data;
	int result;
};

struct _timeout_msg {
	struct _mail_msg msg;
	
	CamelTimeoutCallback cb;
	gpointer camel_data;
};

static void
timeout_timeout (struct _mail_msg *mm)
{
	struct _timeout_msg *m = (struct _timeout_msg *)mm;
	
	/* we ignore the callback result, do we care?? no. */
	m->cb (m->camel_data);
}

static struct _mail_msg_op timeout_op = {
	NULL,
	timeout_timeout,
	NULL,
	NULL,
};

static gboolean 
camel_timeout (gpointer data)
{
	struct _timeout_data *td = data;
	struct _timeout_msg *m;
	
	m = mail_msg_new (&timeout_op, NULL, sizeof (*m));
	
	m->cb = td->cb;
	m->camel_data = td->camel_data;
	
	e_thread_put (mail_thread_queued, (EMsg *)m);
	
	return TRUE;
}

static void
main_register_timeout(struct _timeout_data *td)
{
	td->result = gtk_timeout_add_full(td->interval, camel_timeout, NULL, td, g_free);
}

static guint
register_timeout (CamelSession *session, guint32 interval, CamelTimeoutCallback cb, gpointer camel_data)
{
	struct _timeout_data *td;
	
	/* We do this because otherwise the timeout can get called
	 * more often than the dispatch thread can get rid of it,
	 * leading to timeout calls piling up, and we don't have a
	 * good way to watch the return values. It's not cool.
	 */
	if (interval < 1000) {
		g_warning("Timeout %u too small, increased to 1000", interval);
		interval = 1000;
	}

	/* This is extremely messy, we need to proxy to gtk thread for this */
	td = g_malloc (sizeof (*td));
	td->interval = interval;
	td->result = 0;
	td->cb = cb;
	td->camel_data = camel_data;

	mail_call_main(MAIL_CALL_p_p, (MailMainFunc)main_register_timeout, td);

	if (td->result == 0) {
		g_free(td);
		return 0;
	}

	return td->result;
}

static void
main_remove_timeout(guint *edata)
{
	gtk_timeout_remove(*edata);
}

static gboolean
remove_timeout (CamelSession *session, guint handle)
{
	mail_call_main(MAIL_CALL_p_p, (MailMainFunc)main_remove_timeout, &handle);

	return TRUE;
}

static CamelFolder *
get_folder (CamelFilterDriver *d, const char *uri, void *data, CamelException *ex)
{
	return mail_tool_uri_to_folder (uri, 0, ex);
}

static CamelFilterDriver *
main_get_filter_driver (CamelSession *session, const char *type, CamelException *ex)
{
	CamelFilterDriver *driver;
	RuleContext *fc;
	GString *fsearch, *faction;
	FilterRule *rule = NULL;
	char *user, *system;
	
	user = g_strdup_printf ("%s/filters.xml", evolution_dir);
	system = EVOLUTION_DATADIR "/evolution/filtertypes.xml";
	fc = (RuleContext *)filter_context_new ();
	rule_context_load (fc, system, user);
	g_free (user);
	
	driver = camel_filter_driver_new ();
	camel_filter_driver_set_folder_func (driver, get_folder, NULL);
	
	if (mail_config_get_filter_log ()) {
		MailSession *ms = (MailSession *)session;
		
		if (ms->filter_logfile == NULL) {
			const char *filename;
			
			filename = mail_config_get_filter_log_path ();
			if (filename)
				ms->filter_logfile = fopen (filename, "a+");
		}
		if (ms->filter_logfile)
			camel_filter_driver_set_logfile (driver, ms->filter_logfile);
	}
	
	fsearch = g_string_new ("");
	faction = g_string_new ("");
	
	while ((rule = rule_context_next_rule (fc, rule, type))) {
		g_string_truncate (fsearch, 0);
		g_string_truncate (faction, 0);
		
		filter_rule_build_code (rule, fsearch);
		filter_filter_build_action ((FilterFilter *)rule, faction);
		
		camel_filter_driver_add_rule (driver, rule->name,
					      fsearch->str, faction->str);
	}
	
	g_string_free (fsearch, TRUE);
	g_string_free (faction, TRUE);
	
	gtk_object_unref (GTK_OBJECT (fc));
	return driver;
}

static CamelFilterDriver *
get_filter_driver (CamelSession *session, const char *type, CamelException *ex)
{
	return (CamelFilterDriver *)mail_call_main(MAIL_CALL_p_ppp, (MailMainFunc)main_get_filter_driver,
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

	passwd = e_passwords_get_password (simple_url);

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

	e_passwords_remember_password (simple_url);

	g_free (simple_url);
}

void
mail_session_forget_password (const char *key)
{
	e_passwords_forget_password (key);
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
	g_free (camel_dir);
}

void
mail_session_enable_interaction (gboolean enable)
{
	MAIL_SESSION (session)->interaction_enabled = enable;
}

void
mail_session_forget_passwords (BonoboUIComponent *uih, void *user_data,
			       const char *path)
{
	e_passwords_forget_passwords ();
}
