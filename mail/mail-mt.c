
#include <stdio.h>
#include <unistd.h>

#include "e-util/e-msgport.h"
#include "camel/camel-operation.h"
#include <glib.h>
#include <pthread.h>

#include "mail-mt.h"

#include <gtk/gtk.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnome/gnome-i18n.h>
#include <gal/widgets/e-gui-utils.h>

#include "folder-browser-factory.h"

#include <libgnomeui/gnome-dialog.h>
#include <gtk/gtkprogress.h>

/*#define MALLOC_CHECK*/
#define d(x) 

static void set_view_data(const char *current_message, int busy);
static void set_stop(int sensitive);
static void mail_enable_stop(void);
static void mail_disable_stop(void);
static void mail_operation_status(struct _CamelOperation *op, const char *what, int pc, void *data);

#define MAIL_MT_LOCK(x) pthread_mutex_lock(&x)
#define MAIL_MT_UNLOCK(x) pthread_mutex_unlock(&x)

/* background operation status stuff */
struct _mail_msg_priv {
	GtkProgressBar *bar;
	GtkLabel *label;

	/* for pending requests, before timeout_id is activated (then bar will be ! NULL) */
	char *what;
	int pc;
	int timeout_id;
};

static GtkWindow *progress_dialogue;
static int progress_row;

/* mail_msg stuff */
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
	msg->cancel = camel_operation_new(mail_operation_status, (void *)msg->seq);
	camel_exception_init(&msg->ex);
	msg->priv = g_malloc0(sizeof(*msg->priv));

	g_hash_table_insert(mail_msg_active, (void *)msg->seq, msg);

	d(printf("New message %p\n", msg));

	MAIL_MT_UNLOCK(mail_msg_lock);

	return msg;
}

/* either destroy the progress (in event_data), or the whole dialogue (in data) */
static void destroy_widgets(CamelObject *o, void *event_data, void *data)
{
	if (data)
		gtk_widget_destroy((GtkWidget *)data);
	if (event_data)
		gtk_widget_destroy((GtkWidget *)event_data);
}

#ifdef MALLOC_CHECK
#include <mcheck.h>

static void
checkmem(void *p)
{
	if (p) {
		int status = mprobe(p);

		switch (status) {
		case MCHECK_HEAD:
			printf("Memory underrun at %p\n", p);
			abort();
		case MCHECK_TAIL:
			printf("Memory overrun at %p\n", p);
			abort();
		case MCHECK_FREE:
			printf("Double free %p\n", p);
			abort();
		}
	}
}
#endif

void mail_msg_free(void *msg)
{
	struct _mail_msg *m = msg;

#ifdef MALLOC_CHECK
	checkmem(m);
	checkmem(m->cancel);
	checkmem(m->priv);
#endif
	d(printf("Free message %p\n", msg));

	if (m->ops->destroy_msg)
		m->ops->destroy_msg(m);

	MAIL_MT_LOCK(mail_msg_lock);

	g_hash_table_remove(mail_msg_active, (void *)m->seq);
	pthread_cond_broadcast(&mail_msg_cond);

	/* this closes the bar, and/or the whole progress dialogue, once we're out of things to do */
	if (g_hash_table_size(mail_msg_active) == 0) {
		if (progress_dialogue != NULL) {
			void *data = progress_dialogue;
			progress_dialogue = NULL;
			progress_row = 0;
			mail_proxy_event(destroy_widgets, NULL, data, NULL);
		}
	} else if (m->priv->bar) {
		mail_proxy_event(destroy_widgets, NULL, m->priv->bar, m->priv->label);
	}

	if (m->priv->timeout_id > 0)
		gtk_timeout_remove(m->priv->timeout_id);

	MAIL_MT_UNLOCK(mail_msg_lock);

	camel_operation_unref(m->cancel);
	camel_exception_clear(&m->ex);
	g_free(m->priv->what);
	g_free(m->priv);
	g_free(m);
}

void mail_msg_check_error(void *msg)
{
	struct _mail_msg *m = msg;
	char *what = NULL;
	char *text;
	GnomeDialog *gd;

#ifdef MALLOC_CHECK
	checkmem(m);
	checkmem(m->cancel);
	checkmem(m->priv);
#endif

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
		camel_operation_cancel(m->cancel);

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

#ifdef MALLOC_CHECK
		checkmem(m);
		checkmem(m->cancel);
		checkmem(m->priv);
#endif

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
#ifdef MALLOC_CHECK
		checkmem(m);
		checkmem(m->cancel);
		checkmem(m->priv);
#endif
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

#ifdef MALLOC_CHECK
	checkmem(m);
	checkmem(m->cancel);
	checkmem(m->priv);
#endif	

	if (m->ops->describe_msg)
		mail_status_end();
	mail_msg_free(m);
}

static void
mail_msg_received(EThread *e, EMsg *msg, void *data)
{
	mail_msg_t *m = (mail_msg_t *)msg;

#ifdef MALLOC_CHECK
	checkmem(m);
	checkmem(m->cancel);
	checkmem(m->priv);
#endif

	if (m->ops->describe_msg) {
		char *text = m->ops->describe_msg(m, FALSE);
		d(printf("message received at thread\n"));
		mail_status_start(text);
		g_free(text);
	}

	if (m->ops->receive_msg) {
		mail_enable_stop();
		m->ops->receive_msg(m);
		mail_disable_stop();
	}
}

static void mail_msg_cleanup(void)
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
	const char *prompt;
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
mail_get_password(const char *prompt, gboolean secret)
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

/* ********************************************************************** */

struct _accept_msg {
	struct _mail_msg msg;
	const char *prompt;
	gboolean result;
};

static void
do_get_accept (struct _mail_msg *mm)
{
	struct _accept_msg *m = (struct _accept_msg *)mm;
	GtkWidget *dialog;
	GtkWidget *label;
	
	dialog = gnome_dialog_new (_("Do you accept?"),
				   GNOME_STOCK_BUTTON_YES,
				   GNOME_STOCK_BUTTON_NO,
				   NULL);
	gnome_dialog_set_default (GNOME_DIALOG (dialog), 1);
	
	label = gtk_label_new (m->prompt);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label,
			    TRUE, TRUE, 0);
	
	/* hrm, we can't run this async since the gui_port from which we're called
	   will reply to our message for us */
	m->result = gnome_dialog_run_and_close (GNOME_DIALOG (dialog)) == 0;
}

static void
do_free_accept (struct _mail_msg *mm)
{
	/*struct _accept_msg *m = (struct _accept_msg *)mm;*/
	
	/* nothing to do here */
}

struct _mail_msg_op get_accept_op = {
	NULL,
	do_get_accept,
	NULL,
	do_free_accept,
};

/* prompt the user with a yes/no question and return the response */
gboolean
mail_get_accept (const char *prompt)
{
	struct _accept_msg *m, *r;
	EMsgPort *accept_reply;
	gboolean accept;
	
	accept_reply = e_msgport_new ();
	
	m = mail_msg_new (&get_accept_op, accept_reply, sizeof (*m));
	
	m->prompt = prompt;
	
	if (pthread_self () == mail_gui_thread) {
		do_get_accept ((struct _mail_msg *)m);
		r = m;
	} else {
		static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
		
		/* we want this single-threaded, this is the easiest way to do it without blocking ? */
		pthread_mutex_lock (&lock);
		e_msgport_put (mail_gui_port, (EMsg *)m);
		e_msgport_wait (accept_reply);
		r = (struct _accept_msg *)e_msgport_get (accept_reply);
		pthread_mutex_unlock (&lock);
	}
	
	g_assert (r == m);
	
	accept = m->result;
	
	mail_msg_free (m);
	e_msgport_destroy (accept_reply);
	
	return accept;
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

/* ********************************************************************** */
/* locked via status_lock */
static int busy_state;

static void do_set_busy(struct _mail_msg *mm)
{
	set_stop(busy_state > 0);
}

struct _mail_msg_op set_busy_op = {
	NULL,
	do_set_busy,
	NULL,
	NULL,
};

static void mail_enable_stop(void)
{
	struct _mail_msg *m;

	MAIL_MT_LOCK(status_lock);
	busy_state++;
	if (busy_state == 1) {
		m = mail_msg_new(&set_busy_op, NULL, sizeof(*m));
		e_msgport_put(mail_gui_port, (EMsg *)m);
	}
	MAIL_MT_UNLOCK(status_lock);
}

static void mail_disable_stop(void)
{
	struct _mail_msg *m;

	MAIL_MT_LOCK(status_lock);
	busy_state--;
	if (busy_state == 0) {
		m = mail_msg_new(&set_busy_op, NULL, sizeof(*m));
		e_msgport_put(mail_gui_port, (EMsg *)m);
	}
	MAIL_MT_UNLOCK(status_lock);
}

/* ******************************************************************************** */

struct _op_status_msg {
	struct _mail_msg msg;

	struct _CamelOperation *op;
	char *what;
	int pc;
	void *data;
};

GtkTable *progress_table;

static int op_status_timeout(void *d)
{
	int id = (int)d;
	struct _mail_msg *msg;
	struct _mail_msg_priv *data;
	
	MAIL_MT_LOCK(mail_msg_lock);

	msg = g_hash_table_lookup(mail_msg_active, (void *)id);
	if (msg == NULL) {
		MAIL_MT_UNLOCK(mail_msg_lock);
		return FALSE;
	}

	data = msg->priv;

	if (progress_dialogue == NULL) {
		progress_dialogue = (GtkWindow *)gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_window_set_title(progress_dialogue, _("Evolution progress"));
		gtk_window_set_policy(progress_dialogue, 0, 0, 1);
		gtk_window_set_position(progress_dialogue, GTK_WIN_POS_CENTER);
		progress_table = (GtkTable *)gtk_table_new(1, 2, FALSE);
		gtk_container_add((GtkContainer *)progress_dialogue, (GtkWidget *)progress_table);
	}

	data->bar = (GtkProgressBar *)gtk_progress_bar_new();
	gtk_progress_set_show_text((GtkProgress *)data->bar, TRUE);

	gtk_progress_set_percentage((GtkProgress *)data->bar, (gfloat)(data->pc/100.0));
	gtk_progress_set_format_string((GtkProgress *)data->bar, data->what);

	if (msg->ops->describe_msg) {
		char *desc = msg->ops->describe_msg(msg, FALSE);
		data->label = (GtkLabel *)gtk_label_new(desc);
		g_free(desc);
	} else {
		data->label = (GtkLabel *)gtk_label_new(_("Working"));
	}

	gtk_table_attach(progress_table, (GtkWidget *)data->label, 0, 1, progress_row, progress_row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
	gtk_table_attach(progress_table, (GtkWidget *)data->bar, 1, 2, progress_row, progress_row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
	progress_row++;
	
	gtk_widget_show_all((GtkWidget *)progress_table);
	gtk_widget_show((GtkWidget *)progress_dialogue);

	data->timeout_id = -1;

	MAIL_MT_UNLOCK(mail_msg_lock);

	return FALSE;
}

static void do_op_status(struct _mail_msg *mm)
{
	struct _op_status_msg *m = (struct _op_status_msg *)mm;
	struct _mail_msg *msg;
	struct _mail_msg_priv *data;
	char *out, *p, *o, c;

	g_assert(mail_gui_thread == pthread_self());

	MAIL_MT_LOCK(mail_msg_lock);

	msg = g_hash_table_lookup(mail_msg_active, m->data);
	if (msg == NULL) {
		MAIL_MT_UNLOCK(mail_msg_lock);
		return;
	}

	data = msg->priv;

	out = alloca(strlen(m->what)*2+1);
	o = out;
	p = m->what;
	while ((c = *p++)) {
		if (c=='%')
			*o++ = '%';
		*o++ = c;
	}
	*o = 0;

	if (data->timeout_id == 0) {
		data->what = g_strdup(out);
		data->pc = m->pc;
		data->timeout_id = gtk_timeout_add(2000, op_status_timeout, m->data);
		MAIL_MT_UNLOCK(mail_msg_lock);
		return;
	}

	if (data->bar == NULL) {
		g_free(data->what);
		data->what = g_strdup(out);
		data->pc = m->pc;
		MAIL_MT_UNLOCK(mail_msg_lock);
		return;
	}

	gtk_progress_set_percentage((GtkProgress *)data->bar, (gfloat)(m->pc/100.0));
	gtk_progress_set_format_string((GtkProgress *)data->bar, out);

	MAIL_MT_UNLOCK(mail_msg_lock);
}

static void do_op_status_free(struct _mail_msg *mm)
{
	struct _op_status_msg *m = (struct _op_status_msg *)mm;

	g_free(m->what);
}

struct _mail_msg_op op_status_op = {
	NULL,
	do_op_status,
	NULL,
	do_op_status_free,
};

static void
mail_operation_status(struct _CamelOperation *op, const char *what, int pc, void *data)
{
	struct _op_status_msg *m;

	d(printf("got operation statys: %s %d%%\n", what, pc));

	m = mail_msg_new(&op_status_op, NULL, sizeof(*m));
	m->op = op;
	m->what = g_strdup(what);
	switch (pc) {
	case CAMEL_OPERATION_START:
		pc = 0;
		break;
	case CAMEL_OPERATION_END:
		pc = 100;
		break;
	}
	m->pc = pc;
	m->data = data;
	e_msgport_put(mail_gui_port, (EMsg *)m);
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
				d(printf("clearing msg\n"));
				GNOME_Evolution_ShellView_unsetMessage (shell_view_interface, &ev);
			} else {
				d(printf("setting msg %s\n", current_message ? current_message : "(null)"));
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

static void
set_stop(int sensitive)
{
	EList *controls;
	EIterator *it;
	static int last = FALSE;

	if (last == sensitive)
		return;

	controls = folder_browser_factory_get_control_list ();
	for (it = e_list_get_iterator (controls); e_iterator_is_valid (it); e_iterator_next (it)) {
		BonoboControl *control;
		BonoboUIComponent *uic;

		control = BONOBO_CONTROL (e_iterator_get (it));
		uic = bonobo_control_get_ui_component (control);
		if (uic == CORBA_OBJECT_NIL || bonobo_ui_component_get_container(uic) == CORBA_OBJECT_NIL)
			continue;

		bonobo_ui_component_set_prop(uic, "/commands/MailStop", "sensitive", sensitive?"1":"0", NULL);
	}
	gtk_object_unref(GTK_OBJECT(it));
	last = sensitive;
}
