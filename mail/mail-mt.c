
#include <stdio.h>
#include <unistd.h>

#include "e-util/e-msgport.h"
#include <glib.h>
#include <pthread.h>

#include "mail-mt.h"

#include <gtk/gtk.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnome/gnome-i18n.h>
#include <gal/widgets/e-gui-utils.h>

#include "folder-browser-factory.h"

#define d(x) 

static void set_view_data(const char *current_message, int busy);

#define MAIL_MT_LOCK(x) pthread_mutex_lock(&x)
#define MAIL_MT_UNLOCK(x) pthread_mutex_unlock(&x)

static unsigned int mail_msg_seq; /* sequence number of each message */
static GHashTable *mail_msg_active; /* table of active messages, must hold mail_msg_lock to access */
static pthread_mutex_t mail_msg_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t mail_msg_cond = PTHREAD_COND_INITIALIZER;

pthread_t mail_gui_thread;

void *mail_msg_new(mail_msg_op_t *ops, EMsgPort *reply_port, size_t size)
{
	struct _mail_msg *msg;

	MAIL_MT_LOCK(mail_msg_lock);

	msg = g_malloc0(size);
	msg->ops = ops;
	msg->seq = mail_msg_seq++;
	msg->msg.reply_port = reply_port;
	msg->cancel = camel_cancel_new();
	camel_exception_init(&msg->ex);

	g_hash_table_insert(mail_msg_active, (void *)msg->seq, msg);

	MAIL_MT_UNLOCK(mail_msg_lock);

	return msg;
}

void mail_msg_free(void *msg)
{
	struct _mail_msg *m = msg;

	if (m->ops->destroy_msg)
		m->ops->destroy_msg(m);

	MAIL_MT_LOCK(mail_msg_lock);

	g_hash_table_remove(mail_msg_active, (void *)m->seq);
	pthread_cond_broadcast(&mail_msg_cond);

	MAIL_MT_UNLOCK(mail_msg_lock);

	camel_cancel_unref(m->cancel);
	camel_exception_clear(&m->ex);
	g_free(m);
}

void mail_msg_check_error(void *msg)
{
	struct _mail_msg *m = msg;
	char *what = NULL;
	char *text;
	GnomeDialog *gd;

	if (!camel_exception_is_set(&m->ex)
	    || m->ex.id == CAMEL_EXCEPTION_USER_CANCEL)
		return;

	if (m->ops->describe_msg)
		what = m->ops->describe_msg(m, FALSE);

	if (what)
		text = g_strdup_printf(_("Error while '%s':\n%s"), what, camel_exception_get_description(&m->ex));
	else
		text = g_strdup_printf(_("Error while performing operation:\n%s"), camel_exception_get_description(&m->ex));

	gd = (GnomeDialog *)gnome_error_dialog(text);
	gnome_dialog_run_and_close(gd);
	g_free(text);
}

void mail_msg_cancel(unsigned int msgid)
{
	struct _mail_msg *m;

	MAIL_MT_LOCK(mail_msg_lock);
	m = g_hash_table_lookup(mail_msg_active, (void *)msgid);

	if (m)
		camel_cancel_cancel(m->cancel);

	MAIL_MT_UNLOCK(mail_msg_lock);
}


/* waits for a message to be finished processing (freed)
   the messageid is from struct _mail_msg->seq */
void mail_msg_wait(unsigned int msgid)
{
	struct _mail_msg *m;
	int ismain = pthread_self() == mail_gui_thread;

	if (ismain) {
		MAIL_MT_LOCK(mail_msg_lock);
		m = g_hash_table_lookup(mail_msg_active, (void *)msgid);
		while (m) {
			MAIL_MT_UNLOCK(mail_msg_lock);
			gtk_main_iteration();
			MAIL_MT_LOCK(mail_msg_lock);
			m = g_hash_table_lookup(mail_msg_active, (void *)msgid);
		}
		MAIL_MT_UNLOCK(mail_msg_lock);
	} else {
		MAIL_MT_LOCK(mail_msg_lock);
		m = g_hash_table_lookup(mail_msg_active, (void *)msgid);
		while (m) {
			pthread_cond_wait(&mail_msg_cond, &mail_msg_lock);
			m = g_hash_table_lookup(mail_msg_active, (void *)msgid);
		}
		MAIL_MT_UNLOCK(mail_msg_lock);
	}
}

EMsgPort		*mail_gui_port;
static GIOChannel	*mail_gui_channel;
EMsgPort		*mail_gui_reply_port;
static GIOChannel	*mail_gui_reply_channel;

/* a couple of global threads available */
EThread *mail_thread_queued;	/* for operations that can (or should) be queued */
EThread *mail_thread_new;	/* for operations that should run in a new thread each time */

static gboolean
mail_msgport_replied(GIOChannel *source, GIOCondition cond, void *d)
{
	EMsgPort *port = (EMsgPort *)d;
	mail_msg_t *m;

	while (( m = (mail_msg_t *)e_msgport_get(port))) {
		if (m->ops->reply_msg)
			m->ops->reply_msg(m);
		mail_msg_check_error(m);
		if (m->ops->describe_msg)
			mail_status_end();
		mail_msg_free(m);
	}

	return TRUE;
}

static gboolean
mail_msgport_received(GIOChannel *source, GIOCondition cond, void *d)
{
	EMsgPort *port = (EMsgPort *)d;
	mail_msg_t *m;

	while (( m = (mail_msg_t *)e_msgport_get(port))) {
		if (m->ops->describe_msg) {
			char *text = m->ops->describe_msg(m, FALSE);
			mail_status_start(text);
			g_free(text);
		}
		if (m->ops->receive_msg)
			m->ops->receive_msg(m);
		if (m->msg.reply_port)
			e_msgport_reply((EMsg *)m);
		else {
			if (m->ops->reply_msg)
				m->ops->reply_msg(m);
			if (m->ops->describe_msg)
				mail_status_end();
			mail_msg_free(m);
		}
	}

	return TRUE;
}

static void
mail_msg_destroy(EThread *e, EMsg *msg, void *data)
{
	mail_msg_t *m = (mail_msg_t *)msg;

	if (m->ops->describe_msg)
		mail_status_end();
	mail_msg_free(m);
}

static void
mail_msg_received(EThread *e, EMsg *msg, void *data)
{
	mail_msg_t *m = (mail_msg_t *)msg;

	if (m->ops->describe_msg) {
		char *text = m->ops->describe_msg(m, FALSE);
		d(printf("message received at thread\n"));
		mail_status_start(text);
		g_free(text);
	}

	if (m->ops->receive_msg)
		m->ops->receive_msg(m);
}

void mail_msg_cleanup(void)
{
	e_thread_destroy(mail_thread_queued);
	e_thread_destroy(mail_thread_new);

	e_msgport_destroy(mail_gui_port);
	e_msgport_destroy(mail_gui_reply_port);

	/* FIXME: channels too, etc */
}

void mail_msg_init(void)
{
	mail_gui_reply_port = e_msgport_new();
	mail_gui_reply_channel = g_io_channel_unix_new(e_msgport_fd(mail_gui_reply_port));
	g_io_add_watch(mail_gui_reply_channel, G_IO_IN, mail_msgport_replied, mail_gui_reply_port);

	mail_gui_port = e_msgport_new();
	mail_gui_channel = g_io_channel_unix_new(e_msgport_fd(mail_gui_port));
	g_io_add_watch(mail_gui_channel, G_IO_IN, mail_msgport_received, mail_gui_port);

	mail_thread_queued = e_thread_new(E_THREAD_QUEUE);
	e_thread_set_msg_destroy(mail_thread_queued, mail_msg_destroy, 0);
	e_thread_set_msg_received(mail_thread_queued, mail_msg_received, 0);
	e_thread_set_reply_port(mail_thread_queued, mail_gui_reply_port);

	mail_thread_new = e_thread_new(E_THREAD_NEW);
	e_thread_set_msg_destroy(mail_thread_new, mail_msg_destroy, 0);
	e_thread_set_msg_received(mail_thread_new, mail_msg_received, 0);
	e_thread_set_reply_port(mail_thread_new, mail_gui_reply_port);

	mail_msg_active = g_hash_table_new(NULL, NULL);
	mail_gui_thread = pthread_self();

	atexit(mail_msg_cleanup);
}

/* ********************************************************************** */

struct _set_msg {
	struct _mail_msg msg;
	char *text;
};

/* locks */
static pthread_mutex_t status_lock = PTHREAD_MUTEX_INITIALIZER;
#define STATUS_BUSY_PENDING (2)

/* blah blah */

#define STATUS_DELAY (5)

static int status_depth;
static int status_showing;
static int status_shown;
static char *status_message_next;
static int status_message_clear;
static int status_timeout_id;
/*static int status_busy;*/

struct _status_msg {
	struct _mail_msg msg;
	char *text;
	int busy;
};

static gboolean
status_timeout(void *data)
{
	char *msg;
	int busy = 0;

	d(printf("got status timeout\n"));

	MAIL_MT_LOCK(status_lock);
	if (status_message_next) {
		d(printf("setting message to '%s' busy %d\n", status_message_next, status_busy));
		msg = status_message_next;
		status_message_next = NULL;
		busy = status_depth > 0;
		status_message_clear = 0;
		MAIL_MT_UNLOCK(status_lock);

		/* copy msg so we can set it outside the lock */
		/* unset first is a hack to avoid the stack stuff that doesn't and can't work anyway */
		if (status_shown)
			set_view_data(NULL, FALSE);
		status_shown = TRUE;
		set_view_data(msg, busy);
		g_free(msg);
		return TRUE;
	}

	/* the delay-clear stuff doesn't work yet.  Dont care ... */

	status_showing = FALSE;
	status_message_clear++;
	if (status_message_clear >= STATUS_DELAY && status_depth==0) {
		d(printf("clearing message, busy = %d\n", status_depth));
	} else {
		d(printf("delaying clear\n"));
		MAIL_MT_UNLOCK(status_lock);
		return TRUE;
	}

	status_timeout_id = 0;

	MAIL_MT_UNLOCK(status_lock);

	if (status_shown)
		set_view_data(NULL, FALSE);
	status_shown = FALSE;

	return FALSE;
}

static void do_set_status(struct _mail_msg *mm)
{
	struct _status_msg *m = (struct _status_msg *)mm;

	MAIL_MT_LOCK(status_lock);

	if (status_timeout_id != 0)
		gtk_timeout_remove(status_timeout_id);

	status_timeout_id = gtk_timeout_add(500, status_timeout, 0);
	status_message_clear = 0;

	MAIL_MT_UNLOCK(status_lock);

	/* the 'clear' stuff doesn't really work yet, but oh well,
	   this stuff here needs a little changing for it to work */
	if (status_shown)
		set_view_data(NULL, status_depth != 0);
	status_shown = 0;

	if (m->text) {
		status_shown = 1;
		set_view_data(m->text, status_depth != 0);
	}
}

static void do_del_status(struct _mail_msg *mm)
{
	struct _status_msg *m = (struct _status_msg *)mm;

	g_free(m->text);
}

struct _mail_msg_op set_status_op = {
	NULL,
	do_set_status,
	NULL,
	do_del_status,
};

/* start a new operation */
void mail_status_start(const char *msg)
{
	struct _status_msg *m = NULL;

	MAIL_MT_LOCK(status_lock);
	status_depth++;
	MAIL_MT_UNLOCK(status_lock);

	if (msg == NULL || msg[0] == 0)
		msg = _("Working");

	m = mail_msg_new(&set_status_op, NULL, sizeof(*m));
	m->text = g_strdup(msg);
	m->busy = TRUE;

	e_msgport_put(mail_gui_port, &m->msg.msg);
}

/* end it */
void mail_status_end(void)
{
	struct _status_msg *m = NULL;

	m = mail_msg_new(&set_status_op, NULL, sizeof(*m));
	m->text = NULL;

	MAIL_MT_LOCK(status_lock);
	status_depth--;
	m->busy = status_depth = 0;
	MAIL_MT_UNLOCK(status_lock);

	e_msgport_put(mail_gui_port, &m->msg.msg);
}

/* message during it */
void mail_status(const char *msg)
{
	if (msg == NULL || msg[0] == 0)
		msg = _("Working");

	MAIL_MT_LOCK(status_lock);

	g_free(status_message_next);
	status_message_next = g_strdup(msg);

	MAIL_MT_UNLOCK(status_lock);
}

void mail_statusf(const char *fmt, ...)
{
	va_list ap;
	char *text;

	va_start(ap, fmt);
	text = g_strdup_vprintf(fmt, ap);
	va_end(ap);
	mail_status(text);
	g_free(text);
}

/* ********************************************************************** */

struct _pass_msg {
	struct _mail_msg msg;
	char *prompt;
	int secret;
	char *result;
};

/* libgnomeui's idea of an api/gui is very weird ... hence this dumb hack */
static void focus_on_entry(GtkWidget *widget, void *user_data)
{
	if (GTK_IS_ENTRY(widget))
		gtk_widget_grab_focus(widget);
}

static void pass_got(char *string, void *data)
{
	struct _pass_msg *m = data;

	if (string)
		m->result = g_strdup (string);
}

static void
do_get_pass(struct _mail_msg *mm)
{
	struct _pass_msg *m = (struct _pass_msg *)mm;
	GtkWidget *dialogue;

	/* this api is just awful ... hence the hacks */
	dialogue = gnome_request_dialog(m->secret, m->prompt, NULL,
					0, pass_got, m, NULL);
	e_container_foreach_leaf((GtkContainer *)dialogue, focus_on_entry, NULL);

	/* hrm, we can't run this async since the gui_port from which we're called
	   will reply to our message for us */
	gnome_dialog_run_and_close((GnomeDialog *)dialogue);

	/*gtk_widget_show(dialogue);*/
}

static void
do_free_pass(struct _mail_msg *mm)
{
	/*struct _pass_msg *m = (struct _pass_msg *)mm;*/

	/* the string is passed out so we dont need to free it */
}

struct _mail_msg_op get_pass_op = {
	NULL,
	do_get_pass,
	NULL,
	do_free_pass,
};

/* returns the password, or NULL if cancelled */
char *
mail_get_password(char *prompt, gboolean secret)
{
	char *ret;
	struct _pass_msg *m, *r;
	EMsgPort *pass_reply;

	pass_reply = e_msgport_new();

	m = mail_msg_new(&get_pass_op, pass_reply, sizeof(*m));

	m->prompt = prompt;
	m->secret = secret;

	if (pthread_self() == mail_gui_thread) {
		do_get_pass((struct _mail_msg *)m);
		r = m;
	} else {
		static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

		/* we want this single-threaded, this is the easiest way to do it without blocking ? */
		pthread_mutex_lock(&lock);
		e_msgport_put(mail_gui_port, (EMsg *)m);
		e_msgport_wait(pass_reply);
		r = (struct _pass_msg *)e_msgport_get(pass_reply);
		pthread_mutex_unlock(&lock);
	}

	g_assert(r == m);

	ret = m->result;
	
	mail_msg_free(m);
	e_msgport_destroy(pass_reply);

	return ret;
}

/* ******************** */

struct _proxy_msg {
	struct _mail_msg msg;
	CamelObjectEventHookFunc func;
	CamelObject *o;
	void *event_data;
	void *data;
};

static void
do_proxy_event(struct _mail_msg *mm)
{
	struct _proxy_msg *m = (struct _proxy_msg *)mm;

	m->func(m->o, m->event_data, m->data);
}

struct _mail_msg_op proxy_event_op = {
	NULL,
	do_proxy_event,
	NULL,
	NULL,
};

int mail_proxy_event(CamelObjectEventHookFunc func, CamelObject *o, void *event_data, void *data)
{
	struct _proxy_msg *m;
	int id;
	int ismain = pthread_self() == mail_gui_thread;

	if (ismain) {
		func(o, event_data, data);
		/* id of -1 is 'always finished' */
		return -1;
	} else {
		/* we dont have a reply port for this, we dont care when/if it gets executed, just queue it */
		m = mail_msg_new(&proxy_event_op, NULL, sizeof(*m));
		m->func = func;
		m->o = o;
		m->event_data = event_data;
		m->data = data;
		
		id = m->msg.seq;
		e_msgport_put(mail_gui_port, (EMsg *)m);
		return id;
	}
}

/* ******************** */

/* FIXME FIXME FIXME This is a totally evil hack.  */

static GNOME_Evolution_ShellView
retrieve_shell_view_interface_from_control (BonoboControl *control)
{
	Bonobo_ControlFrame control_frame;
	GNOME_Evolution_ShellView shell_view_interface;
	CORBA_Environment ev;

	control_frame = bonobo_control_get_control_frame (control);

	if (control_frame == NULL)
		return CORBA_OBJECT_NIL;

	CORBA_exception_init (&ev);
	shell_view_interface = Bonobo_Unknown_queryInterface (control_frame,
							       "IDL:GNOME/Evolution/ShellView:1.0",
							       &ev);
	CORBA_exception_free (&ev);

	if (shell_view_interface != CORBA_OBJECT_NIL)
		gtk_object_set_data (GTK_OBJECT (control),
				     "mail_threads_shell_view_interface",
				     shell_view_interface);
	else
		g_warning ("Control frame doesn't have Evolution/ShellView.");

	return shell_view_interface;
}

static void
set_view_data(const char *current_message, int busy)
{
	EList *controls;
	EIterator *it;

	controls = folder_browser_factory_get_control_list ();
	for (it = e_list_get_iterator (controls); e_iterator_is_valid (it); e_iterator_next (it)) {
		BonoboControl *control;
		GNOME_Evolution_ShellView shell_view_interface;
		CORBA_Environment ev;

		control = BONOBO_CONTROL (e_iterator_get (it));

		shell_view_interface = gtk_object_get_data (GTK_OBJECT (control), "mail_threads_shell_view_interface");

		if (shell_view_interface == CORBA_OBJECT_NIL)
			shell_view_interface = retrieve_shell_view_interface_from_control (control);

		CORBA_exception_init (&ev);

		if (shell_view_interface != CORBA_OBJECT_NIL) {
			if ((current_message == NULL || current_message[0] == 0) && ! busy) {
				printf("clearing msg\n");
				GNOME_Evolution_ShellView_unsetMessage (shell_view_interface, &ev);
			} else {
				printf("setting msg %s\n", current_message);
				GNOME_Evolution_ShellView_setMessage (shell_view_interface,
								      current_message?current_message:"",
								      busy,
								      &ev);
			}
		}

		CORBA_exception_free (&ev);

		/* yeah we only set the first one.  Why?  Because it seems to leave
		   random ones lying around otherwise.  Shrug. */
		break;
	}
	gtk_object_unref(GTK_OBJECT(it));
}
