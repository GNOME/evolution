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
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-config.h>
#include <libgnome/gnome-sound.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-messagebox.h>
#include <libgnomeui/gnome-stock.h>

#include <gal/widgets/e-unicode.h>

#include "camel/camel-filter-driver.h"
#include "filter/filter-context.h"
#include "filter/filter-filter.h"
#include "mail.h"
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

static char *get_password(CamelSession *session, const char *prompt, gboolean secret, CamelService *service, const char *item, CamelException *ex);
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

static GnomeDialog *password_dialogue = NULL;
static EDList password_list = E_DLIST_INITIALISER(password_list);
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
		gboolean cache, remember;
		
		m->result = g_strdup (string);
		remember = cache = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (m->check));
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
				mail_config_service_set_save_passwd (service, cache);
				
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
				e_passwords_remember_password (m->key);
		}
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
	GtkWidget *check, *check_label, *entry;
	GList *children, *iter;
	gboolean show;
	char *title;
	unsigned int accel_key;

	/* If we already have a password_dialogue up, save this request till later */
	if (!m->ismain && password_dialogue) {
		e_dlist_addtail(&password_list, (EDListNode *)m);
		return;
	}

	/* FIXME: Remove this total snot */

	/* assume we can use any widget to translate string for display */
	check_label = gtk_label_new ("");
	password_dialogue = (GnomeDialog *)dialogue = gnome_request_dialog (m->secret, m->prompt, NULL, 0, pass_got, m, NULL);
	password_destroy_id = gtk_signal_connect((GtkObject *)dialogue, "destroy", request_password_deleted, m);

	check = gtk_check_button_new ();
	gtk_misc_set_alignment (GTK_MISC (check_label), 0.0, 0.5);
	accel_key = gtk_label_parse_uline (GTK_LABEL (check_label),
					   m->service_url ? _("_Remember this password") :
					   _("_Remember this password for the remainder of this session"));
	gtk_widget_add_accelerator (check, "clicked",
				    GNOME_DIALOG (password_dialogue)->accelerators,
				    accel_key,
				    GDK_MOD1_MASK, 0);
	gtk_container_add (GTK_CONTAINER (check), check_label);

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
		gtk_widget_show_all (check);

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

	if (m->ismain)
		gnome_dialog_run_and_close ((GnomeDialog *)dialogue);
	else
		gtk_widget_show(dialogue);
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
		do_get_pass((struct _mail_msg *)m);
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

/* ********************************************************************** */

static GnomeDialog *message_dialogue;
static EDList message_list = E_DLIST_INITIALISER(password_list);
static guint message_destroy_id;

struct _user_message_msg {
	struct _mail_msg msg;

	CamelSessionAlertType type;
	const char *prompt;

	unsigned int allow_cancel:1;
	unsigned int result:1;
	unsigned int ismain:1;
};

static void do_user_message (struct _mail_msg *mm);

/* if we dont have to wait for reply, we just check to see if any newly waiting prompts are there */
static void
user_message_destroy_noreply(GnomeDialog *gd, void *data)
{
	struct _user_message_msg *m;

	message_dialogue = NULL;
	if ((m = (struct _user_message_msg *)e_dlist_remhead(&message_list)))
		do_user_message((struct _mail_msg *)m);
}

/* clicked, send back the reply */
static void
user_message_clicked(GnomeDialog *gd, int button, struct _user_message_msg *m)
{
	message_dialogue = NULL;

	if (message_destroy_id) {
		gtk_signal_disconnect((GtkObject *)gd, message_destroy_id);
		message_destroy_id = 0;
	}

	m->result = button == 0;
	e_msgport_reply((EMsg *)m);

	/* check for pendings */
	if ((m = (struct _user_message_msg *)e_dlist_remhead(&message_list)))
		do_user_message((struct _mail_msg *)m);
}

static void
user_message_destroy(GnomeDialog *gd, struct _user_message_msg *m)
{
	message_destroy_id = 0;
	user_message_clicked(gd, -1, m);
}

static void
do_user_message (struct _mail_msg *mm)
{
	struct _user_message_msg *m = (struct _user_message_msg *)mm;
	const char *msg_type;

	if (!m->ismain && message_dialogue != NULL) {
		e_dlist_addtail(&message_list, (EDListNode *)m);
		return;
	}

	switch (m->type) {
	case CAMEL_SESSION_ALERT_INFO:
		msg_type = GNOME_MESSAGE_BOX_INFO;
		break;
	case CAMEL_SESSION_ALERT_WARNING:
		msg_type = GNOME_MESSAGE_BOX_WARNING;
		break;
	case CAMEL_SESSION_ALERT_ERROR:
		msg_type = GNOME_MESSAGE_BOX_ERROR;
		break;
	default:
		msg_type = NULL;
	}

	message_dialogue = (GnomeDialog *)gnome_message_box_new(m->prompt, msg_type, GNOME_STOCK_BUTTON_OK,
								m->allow_cancel ? GNOME_STOCK_BUTTON_CANCEL : NULL,
								NULL);
	gnome_dialog_set_default(message_dialogue, 1);
	gnome_dialog_set_close(message_dialogue, TRUE);
	gtk_window_set_policy (GTK_WINDOW (message_dialogue), TRUE, TRUE, TRUE);

	/* We only need to wait for the result if we allow cancel otherwise show but send result back instantly */
	if (m->allow_cancel) {
		gtk_signal_connect((GtkObject*)message_dialogue, "clicked", user_message_clicked, m);
		message_destroy_id = gtk_signal_connect((GtkObject*)message_dialogue, "destroy", user_message_destroy, m);
		if (m->ismain)
			gnome_dialog_run_and_close ((GnomeDialog *)message_dialogue);
		else
			gtk_widget_show((GtkWidget *)message_dialogue);
	} else {
		gtk_signal_connect((GtkObject *)message_dialogue, "destroy", user_message_destroy_noreply, NULL);
		gtk_widget_show((GtkWidget *)message_dialogue);
		m->result = TRUE;
		e_msgport_reply((EMsg *)m);
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
timeout_done(struct _mail_msg *mm)
{
	struct _timeout_msg *m = (struct _timeout_msg *)mm;
	MailSession *ms = (MailSession *)m->session;
	struct _timeout_data *td;

	if (!m->result) {
		MAIL_SESSION_LOCK(ms, lock);
		td = find_timeout(&ms->timeouts, m->id);
		if (td) {
			e_dlist_remove((EDListNode *)td);
			if (td->timeout_id)
				gtk_timeout_remove(td->timeout_id);
			g_free(td);
		}
		MAIL_SESSION_UNLOCK(ms, lock);
	}
}

static void
timeout_free(struct _mail_msg *mm)
{
	struct _timeout_msg *m = (struct _timeout_msg *)mm;

	camel_object_unref((CamelObject *)m->session);
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
	
	m = mail_msg_new(&timeout_op, NULL, sizeof (*m));

	m->session = td->session;
	camel_object_ref((CamelObject *)td->session);
	m->id = td->id;
	
	e_thread_put(mail_thread_queued, (EMsg *)m);
	
	return TRUE;
}

static void
main_register_timeout(CamelSession *session, void *event_data, void *data)
{
	MailSession *ms = (MailSession *)session;
	unsigned int handle = (unsigned int)event_data;
	struct _timeout_data *td;

	MAIL_SESSION_LOCK(session, lock);
	td = find_timeout(&ms->timeouts, handle);
	if (td) {
		if (td->removed) {
			e_dlist_remove((EDListNode *)td);
			if (td->timeout_id)
				gtk_timeout_remove(td->timeout_id);
			g_free(td);
		} else {
			td->timeout_id = gtk_timeout_add(td->interval, camel_timeout, td);
		}
	}
	MAIL_SESSION_UNLOCK(session, lock);

	camel_object_unref((CamelObject *)ms);
}

static guint
register_timeout (CamelSession *session, guint32 interval, CamelTimeoutCallback cb, gpointer camel_data)
{
	struct _timeout_data *td;
	MailSession *ms = (MailSession *)session;
	guint ret;

	MAIL_SESSION_LOCK(session, lock);

	ret = ms->timeout_id;
	ms->timeout_id ++;

	/* just debugging, the timeout code now ignores excessive events anyway */
	if (interval < 100)
		g_warning("Timeout requested %d is small, may cause performance problems", interval);

	td = g_malloc(sizeof(*td));
	td->cb = cb;
	td->camel_data = camel_data;
	td->interval = interval;
	td->id = ret;
	td->session = session;
	td->removed = FALSE;
	td->busy = FALSE;
	e_dlist_addhead(&ms->timeouts, (EDListNode *)td);

	MAIL_SESSION_UNLOCK(session, lock);

	camel_object_ref((CamelObject *)ms);
	mail_async_event_emit(ms->async, MAIL_ASYNC_GUI, (MailAsyncFunc)main_register_timeout, (CamelObject *)session, (void *)ret, NULL);

	return ret;
}

static void
main_remove_timeout(CamelSession *session, void *event_data, void *data)
{
	MailSession *ms = (MailSession *)session;
	unsigned int handle = (unsigned int)event_data;
	struct _timeout_data *td;

	MAIL_SESSION_LOCK(session, lock);
	td = find_timeout(&ms->timeouts, handle);
	if (td) {
		e_dlist_remove((EDListNode *)td);
		if (td->timeout_id)
			gtk_timeout_remove(td->timeout_id);
		g_free(td);
	}
	MAIL_SESSION_UNLOCK(session, lock);

	camel_object_unref((CamelObject *)ms);
}

static gboolean
remove_timeout (CamelSession *session, guint handle)
{
	MailSession *ms = (MailSession *)session;
	struct _timeout_data *td;
	int remove = FALSE;

	MAIL_SESSION_LOCK(session, lock);
	td = find_timeout(&ms->timeouts, handle);
	if (td && !td->removed) {
		td->removed = TRUE;
		remove = TRUE;
	}
	MAIL_SESSION_UNLOCK(session, lock);

	if (remove) {
		camel_object_ref((CamelObject *)ms);
		mail_async_event_emit(ms->async, MAIL_ASYNC_GUI,
				      (MailAsyncFunc)main_remove_timeout, (CamelObject *)session, (void *)handle, NULL);
	} else
		g_warning("Removing a timeout i dont know about (or twice): %d", handle);

	return TRUE;
}

static CamelFolder *
get_folder (CamelFilterDriver *d, const char *uri, void *data, CamelException *ex)
{
	return mail_tool_uri_to_folder (uri, 0, ex);
}

static void
session_play_sound (CamelFilterDriver *driver, const char *filename, gpointer user_data)
{
	if (!filename || !*filename)
		gdk_beep ();
	else
		gnome_sound_play (filename);
}

static void
session_system_beep (CamelFilterDriver *driver, gpointer user_data)
{
	gdk_beep ();
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
	
	driver = camel_filter_driver_new (session);
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
	
	camel_filter_driver_set_shell_func (driver, mail_execute_shell_command, NULL);
	camel_filter_driver_set_play_sound_func (driver, session_play_sound, NULL);
	camel_filter_driver_set_system_beep_func (driver, session_system_beep, NULL);
	
	fsearch = g_string_new ("");
	faction = g_string_new ("");
	
	/* add the new-mail notification rule first to be sure that it gets invoked */
	
	/* FIXME: we need a way to distinguish between filtering new
           mail and re-filtering a folder because both use the
           "incoming" filter type */
	if (mail_config_get_new_mail_notify () && !strcmp (type, "incoming")) {
		g_string_truncate (faction, 0);
		
		g_string_append (faction, "(only-once \"new-mail-notification\" ");
		
		switch (mail_config_get_new_mail_notify ()) {
		case MAIL_CONFIG_NOTIFY_BEEP:
			g_string_append (faction, "\"(beep)\"");
			break;
		case MAIL_CONFIG_NOTIFY_PLAY_SOUND:
			g_string_sprintfa (faction, "\"(play-sound \\\"%s\\\")\"",
					   mail_config_get_new_mail_notify_sound_file ());
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

	/* The shell will tell us to go online. */
	camel_session_set_online ((CamelSession *)session, FALSE);

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

		d(printf("Gone non-interactive, checking for outstanding interactive tasks\n"));

		/* clear out pending password requests */
		while ((pm = (struct _pass_msg *)e_dlist_remhead(&password_list))) {
			d(printf("Flushing password request : %s\n", pm->prompt));
			e_msgport_reply((EMsg *)pm);
		}

		/* destroy the current */
		if (password_dialogue) {
			d(printf("Destroying password dialogue\n"));
			gtk_object_destroy((GtkObject *)password_dialogue);
		}

		/* same for pending user messages */
		while ((um = (struct _user_message_msg *)e_dlist_remhead(&message_list))) {
			d(printf("Flusing message request: %s\n", um->prompt));
			e_msgport_reply((EMsg *)um);
		}

		/* and the current */
		if (message_dialogue) {
			d(printf("Destroying message dialogue\n"));
			gtk_object_destroy((GtkObject *)message_dialogue);
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
