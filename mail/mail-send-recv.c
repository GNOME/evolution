
#include <stdio.h>
#include <string.h>

#include "filter/filter-context.h"
#include "filter/filter-filter.h"
#include "camel/camel-filter-driver.h"
#include "camel/camel-folder.h"
#include "camel/camel-session.h"
#include "camel/camel-uid-cache.h"

#include "evolution-storage.h"

#include "mail-mt.h"
#include "mail-config.h"
#include "mail-session.h"

/* for the dialogue stuff */
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-dialog.h>

/* send/receive email */

extern char *evolution_dir;
extern CamelFolder *mail_tool_filter_get_folder_func (CamelFilterDriver *d, const char *uri, void *data);
extern CamelFolder *mail_tool_uri_to_folder(const char *uri, CamelException *ex);

static FilterContext *
load_context(void)
{
	char *userrules;
	char *systemrules;
	FilterContext *fc;
	
	userrules = g_strdup_printf ("%s/filters.xml", evolution_dir);
	systemrules = g_strdup_printf ("%s/evolution/filtertypes.xml", EVOLUTION_DATADIR);
	fc = filter_context_new ();
	rule_context_load ((RuleContext *)fc, systemrules, userrules);
	g_free (userrules);
	g_free (systemrules);
	
	return fc;
}

static void
setup_filter_driver(CamelFilterDriver *driver, FilterContext *fc, const char *source)
{
	GString *fsearch, *faction;
	FilterFilter *rule = NULL;

	if (TRUE /* perform_logging */) {
		char *filename = g_strdup_printf("%s/evolution-filter-log", evolution_dir);
		FILE *logfile = fopen(filename, "a+");
		g_free(filename);
		if (logfile)
			camel_filter_driver_set_logfile(driver, logfile);
	}

	fsearch = g_string_new ("");
	faction = g_string_new ("");
	
	while ((rule = (FilterFilter *)rule_context_next_rule((RuleContext *)fc, (FilterRule *)rule, source))) {
		g_string_truncate (fsearch, 0);
		g_string_truncate (faction, 0);
		
		filter_rule_build_code ((FilterRule *)rule, fsearch);
		filter_filter_build_action (rule, faction);

		camel_filter_driver_add_rule(driver, ((FilterRule *)rule)->name, fsearch->str, faction->str);
	}
	
	g_string_free (fsearch, TRUE);
	g_string_free (faction, TRUE);
}

static CamelFolder *
filter_get_folder(CamelFilterDriver *d, const char *uri, void *data, CamelException *ex)
{
	CamelFolder *folder;

	folder = mail_tool_uri_to_folder(uri, ex);

	return folder;
}

/* used for both just filtering a folder + uid's, and for filtering a whole folder */
/* used both for fetching mail, and for filtering mail */
struct _filter_mail_msg {
	struct _mail_msg msg;

	CamelFolder *source_folder; /* where they come from */
	GPtrArray *source_uids;	/* uids to copy, or NULL == copy all */
	CamelCancel *cancel;
	CamelFilterDriver *driver;
	int delete;		/* delete messages after filtering them? */
	CamelFolder *destination; /* default destination for any messages, NULL for none */
};

/* since fetching also filters, we subclass the data here */
struct _fetch_mail_msg {
	struct _filter_mail_msg fmsg;

	CamelCancel *cancel;	/* we have our own cancellation struct, the other should be empty */
	int keep;		/* keep on server? */

	char *source_uri;

	void (*done)(char *source, void *data);
	void *data;
};

/* filter a folder, or a subset thereof, uses source_folder/source_uids */
/* this is shared with fetch_mail */
static void
filter_folder_filter(struct _mail_msg *mm)
{
	struct _filter_mail_msg *m = (struct _filter_mail_msg *)mm;
	CamelFolder *folder;
	GPtrArray *uids, *folder_uids = NULL;

	if (m->cancel)
		camel_cancel_register(m->cancel);

	folder = m->source_folder;

	if (folder == NULL || camel_folder_get_message_count (folder) == 0) {
		if (m->cancel)
			camel_cancel_unregister(m->cancel);
		return;
	}

	if (m->destination) {
		camel_folder_freeze(m->destination);
		camel_filter_driver_set_default_folder(m->driver, m->destination);
	}

	camel_folder_freeze(folder);

	if (m->source_uids)
		uids = m->source_uids;
	else
		folder_uids = uids = camel_folder_get_uids (folder);

	camel_filter_driver_filter_folder(m->driver, folder, uids, m->delete, &mm->ex);

	if (folder_uids)
		camel_folder_free_uids(folder, folder_uids);

	/* sync and expunge */
	camel_folder_sync (folder, TRUE, &mm->ex);
	camel_folder_thaw(folder);

	if (m->destination)
		camel_folder_thaw(m->destination);

	if (m->cancel)
		camel_cancel_unregister(m->cancel);
}

static void
filter_folder_filtered(struct _mail_msg *mm)
{
}

static void
filter_folder_free(struct _mail_msg *mm)
{
	struct _filter_mail_msg *m = (struct _filter_mail_msg *)mm;
	int i;

	if (m->source_folder)
		camel_object_unref((CamelObject *)m->source_folder);
	if (m->source_uids) {
		for (i=0;i<m->source_uids->len;i++)
			g_free(m->source_uids->pdata[i]);
		g_ptr_array_free(m->source_uids, TRUE);
	}
	if (m->cancel)
		camel_cancel_unref(m->cancel);
	if (m->destination)
		camel_object_unref((CamelObject *)m->destination);
	camel_object_unref((CamelObject *)m->driver);
}

static struct _mail_msg_op filter_folder_op = {
	NULL,			/* we do our own progress reporting? */
	filter_folder_filter,
	filter_folder_filtered,
	filter_folder_free,
};

void
mail_filter_folder(CamelFolder *source_folder, GPtrArray *uids,
		   FilterContext *fc, const char *type,
		   CamelCancel *cancel)
{
	struct _filter_mail_msg *m;

	m = mail_msg_new(&filter_folder_op, NULL, sizeof(*m));
	m->source_folder = source_folder;
	camel_object_ref((CamelObject *)source_folder);
	m->source_uids = uids;
	m->delete = FALSE;
	if (cancel) {
		m->cancel = cancel;
		camel_cancel_ref(cancel);
	}

	m->driver = camel_filter_driver_new(filter_get_folder, NULL);
	setup_filter_driver(m->driver, fc, type);

	e_thread_put(mail_thread_new, (EMsg *)m);
}

/* ********************************************************************** */

static void
fetch_mail_fetch(struct _mail_msg *mm)
{
	struct _fetch_mail_msg *m = (struct _fetch_mail_msg *)mm;
	struct _filter_mail_msg *fm = (struct _filter_mail_msg *)mm;
	int i;

	if (m->cancel)
		camel_cancel_register(m->cancel);

	if ( (fm->destination = mail_tool_get_local_inbox(&mm->ex)) == NULL) {
		if (m->cancel)
			camel_cancel_unregister(m->cancel);
		return;
	}

	/* FIXME: this should support keep_on_server too, which would then perform a spool
	   access thingy, right?  problem is matching raw messages to uid's etc. */
	if (!strncmp (m->source_uri, "mbox:", 5)) {
		char *path = mail_tool_do_movemail (m->source_uri, &mm->ex);
		
		if (path && !camel_exception_is_set (&mm->ex)) {
			camel_filter_driver_filter_mbox(fm->driver, path, &mm->ex);
			
			if (!camel_exception_is_set (&mm->ex))
				unlink (path);
		}
		g_free (path);
	} else {
		CamelFolder *folder = fm->source_folder = mail_tool_get_inbox(m->source_uri, &mm->ex);
		CamelUIDCache *cache = NULL;

		if (folder) {
			/* this handles 'keep on server' stuff, if we have any new uid's to copy
			   across, we need to copy them to a new array 'cause of the way fetch_mail_free works */
			if (!fm->delete) {
				char *cachename = mail_config_folder_to_cachename (folder, "cache-");
		
				cache = camel_uid_cache_new (cachename);
				if (cache) {
					GPtrArray *folder_uids, *cache_uids, *uids;
					
					folder_uids = camel_folder_get_uids(folder);
					cache_uids = camel_uid_cache_get_new_uids(cache, folder_uids);
					if (cache_uids) {
						/* need to copy this, sigh */
						fm->source_uids = uids = g_ptr_array_new();
						g_ptr_array_set_size(uids, cache_uids->len);
						for (i=0;i<cache_uids->len;i++)
							uids->pdata[i] = g_strdup(cache_uids->pdata[i]);
						camel_uid_cache_free_uids (cache_uids);

						filter_folder_filter(mm);
						if (!camel_exception_is_set (&mm->ex))
							camel_uid_cache_save (cache);
						camel_uid_cache_destroy (cache);
					}
					camel_folder_free_uids(folder, folder_uids);
				}
				g_free (cachename);
			} else {
				filter_folder_filter(mm);
			}
		}

	}

	if (m->cancel)
		camel_cancel_unregister(m->cancel);
}

static void
fetch_mail_fetched(struct _mail_msg *mm)
{
	struct _fetch_mail_msg *m = (struct _fetch_mail_msg *)mm;

	if (m->done)
		m->done(m->source_uri, m->data);
}

static void
fetch_mail_free(struct _mail_msg *mm)
{
	struct _fetch_mail_msg *m = (struct _fetch_mail_msg *)mm;

	g_free(m->source_uri);
	if (m->cancel)
		camel_cancel_unref(m->cancel);

	filter_folder_free(mm);
}

static struct _mail_msg_op fetch_mail_op = {
	NULL,			/* we do our own progress reporting */
	fetch_mail_fetch,
	fetch_mail_fetched,
	fetch_mail_free,
};

/* ouch, a 'do everything' interface ... */
void mail_fetch_mail(const char *source, int keep,
		     FilterContext *fc, const char *type,
		     CamelCancel *cancel,
		     CamelFilterGetFolderFunc get_folder, void *get_data,
		     CamelFilterStatusFunc *status, void *status_data,
		     void (*done)(char *source, void *data), void *data)
{
	struct _fetch_mail_msg *m;
	struct _filter_mail_msg *fm;

	m = mail_msg_new(&fetch_mail_op, NULL, sizeof(*m));
	fm = (struct _filter_mail_msg *)m;
	m->source_uri = g_strdup(source);
	fm->delete = !keep;
	if (cancel) {
		m->cancel = cancel;
		camel_cancel_ref(cancel);
	}
	m->done = done;
	m->data = data;

	fm->driver = camel_filter_driver_new(get_folder, get_data);
	setup_filter_driver(fm->driver, fc, type);
	if (status)
		camel_filter_driver_set_status_func(fm->driver, status, status_data);

	e_thread_put(mail_thread_new, (EMsg *)m);
}


/* updating of imap folders etc */
struct _update_info {
	EvolutionStorage *storage;

	void (*done)(CamelStore *, void *data);
	void *data;
};

static void do_update_subfolders_rec(CamelStore *store, CamelFolderInfo *info, EvolutionStorage *storage, const char *prefix)
{
	char *path, *name;
	
	path = g_strdup_printf("%s/%s", prefix, info->name);
	if (info->unread_message_count > 0)
		name = g_strdup_printf("%s (%d)", info->name, info->unread_message_count);
	else
		name = g_strdup(info->name);
	
	evolution_storage_update_folder(storage, path, name, info->unread_message_count > 0);
	g_free(name);
	if (info->child)
		do_update_subfolders_rec(store, info->child, storage, path);
	if (info->sibling)
		do_update_subfolders_rec(store, info->sibling, storage, prefix);
	g_free(path);
}

static void do_update_subfolders(CamelStore *store, CamelFolderInfo *info, void *data)
{
	struct _update_info *uinfo = data;
	
	if (uinfo) {
		do_update_subfolders_rec(store, info, uinfo->storage, "");
	}

	if (uinfo->done)
		uinfo->done(store, uinfo->data);

	gtk_object_unref((GtkObject *)uinfo->storage);
	g_free(uinfo);
}

/* this interface is a little icky */
int mail_update_subfolders(CamelStore *store, EvolutionStorage *storage,
			   void (*done)(CamelStore *, void *data), void *data)
{
	struct _update_info *info;

	/* FIXME: This wont actually work entirely right, as a failure may lose this data */
	/* however, this isn't a big problem ... */
	info = g_malloc0(sizeof(*info));
	info->storage = storage;
	gtk_object_ref((GtkObject *)storage);
	info->done = done;
	info->data = data;

	return mail_get_folderinfo(store, do_update_subfolders, info);
}

/* ********************************************************************** */
/* sending stuff */
/* ** SEND MAIL *********************************************************** */

/* send 1 message to a specific transport */
static void
mail_send_message(CamelMimeMessage *message, const char *destination, CamelFilterDriver *driver, CamelException *ex)
{
	extern CamelFolder *sent_folder; /* FIXME */
	CamelMessageInfo *info;
	CamelTransport *xport;
	const char *version;

	if (SUB_VERSION[0] == '\0')
		version = "Evolution (" VERSION " - Preview Release)";
	else
		version = "Evolution (" VERSION "/" SUB_VERSION " - Preview Release)";
	camel_medium_add_header(CAMEL_MEDIUM (message), "X-Mailer", version);
	camel_mime_message_set_date(message, CAMEL_MESSAGE_DATE_CURRENT, 0);

	xport = camel_session_get_transport(session, destination, ex);
	if (camel_exception_is_set(ex))
		return;
	
	mail_tool_send_via_transport(xport, (CamelMedium *)message, ex);
	camel_object_unref((CamelObject *)xport);
	if (camel_exception_is_set(ex))
		return;
	
	/* post-process */
	info = camel_message_info_new();
	info->flags = CAMEL_MESSAGE_SEEN;

	if (driver)
		camel_filter_driver_filter_message(driver, message, info, "", ex);
	
	if (sent_folder)
		camel_folder_append_message(sent_folder, message, info, ex);
	
	camel_message_info_free(info);
}

/* ********************************************************************** */

struct _send_mail_msg {
	struct _mail_msg msg;

	CamelFilterDriver *driver;
	char *destination;
	CamelMimeMessage *message;

	void (*done)(char *uri, CamelMimeMessage *message, gboolean sent, void *data);
	void *data;
};

static char *send_mail_desc(struct _mail_msg *mm, int done)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;
	const char *subject;

	subject = camel_mime_message_get_subject(m->message);
	if (subject && subject[0])
		return g_strdup_printf (_("Sending \"%s\""), subject);
	else
		return g_strdup(_("Sending message"));
}

static void send_mail_send(struct _mail_msg *mm)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;

	camel_cancel_register(mm->cancel);
	mail_send_message(m->message, m->destination, m->driver, &mm->ex);
	camel_cancel_unregister(mm->cancel);
}

static void send_mail_sent(struct _mail_msg *mm)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;

	if (m->done)
		m->done(m->destination, m->message, !camel_exception_is_set(&mm->ex), m->data);
}

static void send_mail_free(struct _mail_msg *mm)
{
	struct _send_mail_msg *m = (struct _send_mail_msg *)mm;

	camel_object_unref((CamelObject *)m->message);
	g_free(m->destination);
}

static struct _mail_msg_op send_mail_op = {
	send_mail_desc,
	send_mail_send,
	send_mail_sent,
	send_mail_free,
};

int
mail_send_mail(const char *uri, CamelMimeMessage *message, void (*done) (char *uri, CamelMimeMessage *message, gboolean sent, void *data), void *data)
{
	struct _send_mail_msg *m;
	int id;
	FilterContext *fc;

	m = mail_msg_new(&send_mail_op, NULL, sizeof(*m));
	m->destination = g_strdup(uri);
	m->message = message;
	camel_object_ref((CamelObject *)message);
	m->data = data;
	m->done = done;

	id = m->msg.seq;

	m->driver = camel_filter_driver_new(filter_get_folder, NULL);
	fc = load_context();
	setup_filter_driver(m->driver, fc, FILTER_SOURCE_OUTGOING);
	gtk_object_unref((GtkObject *)fc);

	e_thread_put(mail_thread_new, (EMsg *)m);
	return id;
}

/* ** SEND MAIL QUEUE ***************************************************** */

struct _send_queue_msg {
	struct _mail_msg msg;

	CamelFolder *queue;
	char *destination;

	CamelFilterDriver *driver;
	CamelCancel *cancel;

	/* we use camelfilterstatusfunc, even though its not the filter doing it */
	CamelFilterStatusFunc *status;
	void *status_data;

	void (*done)(char *destination, void *data);
	void *data;
};

static void
report_status(struct _send_queue_msg *m, enum camel_filter_status_t status, int pc, const char *desc, ...)
{
	va_list ap;
	char *str;
	
	if (m->status) {
		va_start(ap, desc);
		str = g_strdup_vprintf(desc, ap);
		m->status(m->driver, status, pc, str, m->status_data);
		g_free(str);
	}
}

static void
send_queue_send(struct _mail_msg *mm)
{
	struct _send_queue_msg *m = (struct _send_queue_msg *)mm;
	GPtrArray *uids;
	int i;
	extern CamelFolder *sent_folder; /* FIXME */

	printf("sending queue\n");

	uids = camel_folder_get_uids(m->queue);
	if (uids == NULL || uids->len == 0)
		return;

	if (m->cancel)
		camel_cancel_register(m->cancel);
	
	for (i=0; i<uids->len; i++) {
		CamelMimeMessage *message;
		char *destination;
		int pc = (100 * i)/uids->len;

		report_status(m, FILTER_STATUS_START, pc, "Sending message %d of %d", i+1, uids->len);
		
		message = camel_folder_get_message(m->queue, uids->pdata[i], &mm->ex);
		if (camel_exception_is_set(&mm->ex))
			break;

		/* Get the preferred transport URI */
		destination = (char *)camel_medium_get_header(CAMEL_MEDIUM(message), "X-Evolution-Transport");
		if (destination) {
			destination = g_strdup(destination);
			camel_medium_remove_header(CAMEL_MEDIUM(message), "X-Evolution-Transport");
			mail_send_message(message, g_strstrip(destination), m->driver, &mm->ex);
			g_free(destination);
		} else
			mail_send_message(message, m->destination, m->driver, &mm->ex);

		if (camel_exception_is_set(&mm->ex))
			break;

		camel_folder_set_message_flags(m->queue, uids->pdata[i], CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
	}

	if (camel_exception_is_set(&mm->ex))
		report_status(m, FILTER_STATUS_END, 100, "Failed on message %d of %d", i+1, uids->len);
	else
		report_status(m, FILTER_STATUS_END, 100, "Complete.");

	camel_folder_free_uids(m->queue, uids);

	if (!camel_exception_is_set(&mm->ex))
		camel_folder_expunge(m->queue, &mm->ex);
	
	if (sent_folder)
		camel_folder_sync(sent_folder, FALSE, &mm->ex);

	if (m->cancel)
		camel_cancel_unregister(m->cancel);
}

static void
send_queue_sent(struct _mail_msg *mm)
{
	struct _send_queue_msg *m = (struct _send_queue_msg *)mm;

	if (m->done)
		m->done(m->destination, m->data);
}

static void
send_queue_free(struct _mail_msg *mm)
{
	struct _send_queue_msg *m = (struct _send_queue_msg *)mm;
	
	camel_object_unref((CamelObject *)m->queue);
	g_free(m->destination);
	if (m->cancel)
		camel_cancel_unref(m->cancel);
}

static struct _mail_msg_op send_queue_op = {
	NULL,			/* do our own reporting, as with fetch mail */
	send_queue_send,
	send_queue_sent,
	send_queue_free,
};

/* same interface as fetch_mail, just 'cause i'm lazy today (and we need to run it from the same spot?) */
void
mail_send_queue(CamelFolder *queue, const char *destination,
		FilterContext *fc, const char *type,
		CamelCancel *cancel,
		CamelFilterGetFolderFunc get_folder, void *get_data,
		CamelFilterStatusFunc *status, void *status_data,
		void (*done)(char *destination, void *data), void *data)
{
	struct _send_queue_msg *m;

	m = mail_msg_new(&send_queue_op, NULL, sizeof(*m));
	m->queue = queue;
	camel_object_ref((CamelObject *)queue);
	m->destination = g_strdup(destination);
	if (cancel) {
		m->cancel = cancel;
		camel_cancel_ref(cancel);
	}
	m->status = status;
	m->status_data = status_data;
	m->done = done;
	m->data = data;

	m->driver = camel_filter_driver_new(get_folder, get_data);
	setup_filter_driver(m->driver, fc, type);

	e_thread_put(mail_thread_new, (EMsg *)m);
}


/* ********************************************************************** */
/*  This stuff below is independent of the stuff above */

/* this stuff is used to keep track of which folders filters have accessed, and
   what not. the thaw/refreeze thing doesn't really seem to work though */
struct _folder_info {
	char *uri;
	CamelFolder *folder;
	time_t update;
	int count;		/* how many times updated, to slow it down as we go, if we have lots */
};

struct _send_data {
	GList *infos;

	int active;		/* how many still active */

	GnomeDialog *gd;
	int cancelled;

	CamelFolder *inbox;	/* since w'ere never asked to uypdate this one, do it ourselves */
	time_t inbox_update;

	GMutex *lock;
	GHashTable *folders;
};

typedef enum {
	SEND_RECEIVE,		/* receiver */
	SEND_SEND,		/* sender */
	SEND_UPDATE,		/* imap-like 'just update folder info' */
} send_info_t ;

typedef enum {
	SEND_ACTIVE,
	SEND_CANCELLED,
	SEND_COMPLETE
} send_state_t;

struct _send_info {
	send_info_t type;		/* 0 = fetch, 1 = send */
	CamelCancel *cancel;
	char *uri;
	int keep;
	send_state_t state;
	GtkProgressBar *bar;
	GtkButton *stop;
	time_t update;
	struct _send_data *data;
};

static void
receive_cancel(GtkButton *button, struct _send_info *info)
{
	if (info->state == SEND_ACTIVE) {
		camel_cancel_cancel(info->cancel);
		gtk_progress_set_format_string((GtkProgress *)info->bar, _("Cancelling ..."));
		info->state = SEND_CANCELLED;
	}
	gtk_widget_set_sensitive((GtkWidget *)info->stop, FALSE);
}

static void
free_folder_info(void *key, struct _folder_info *info, void *data)
{
	camel_object_unref((CamelObject *)info->folder);
	g_free(info->uri);
}

static void
free_info_data(void *datain)
{
	struct _send_data *data = datain;
	GList *list = data->infos;

	while (list) {
		struct _send_info *info = list->data;
		g_free(info->uri);
		camel_cancel_unref(info->cancel);
		list = list->next;
	}

	g_list_free(data->infos);
	g_hash_table_foreach(data->folders, (GHFunc)free_folder_info, NULL);
	g_hash_table_destroy(data->folders);
	g_mutex_free(data->lock);
	if (data->inbox)
		camel_object_unref((CamelObject *)data->inbox);
	g_free(data);
}

static void
dialogue_clicked(GnomeDialog *gd, int button, struct _send_data *data)
{
	GList *scan;

	switch(button) {
	case 0:			/* ok */
		gnome_dialog_close(gd);
		break;
	case 1:
		printf("cancelled whole thing\n");
		if (!data->cancelled) {
			data->cancelled = TRUE;
			scan = data->infos;
			while (scan) {
				struct _send_info *info = scan->data;
				receive_cancel(info->stop, info);
				scan = scan->next;
			}
		}
		gnome_dialog_set_sensitive(gd, 1, FALSE);
		break;
	}
}

static struct _send_data *build_dialogue(GSList *sources, CamelFolder *outbox, const char *destination)
{
	GnomeDialog *gd;
	GtkFrame *frame;
	GtkTable *table;
	int row;
	GList *list = NULL;
	struct _send_data *data;
	GtkLabel *label;
	GtkProgressBar *bar;
	GtkButton *stop;
	struct _send_info *info;
	
	data = g_malloc0(sizeof(*data));
	data->lock = g_mutex_new();
	data->folders = g_hash_table_new(g_str_hash, g_str_equal);
	data->inbox = mail_tool_get_local_inbox(NULL);

	gd = (GnomeDialog *)gnome_dialog_new(_("Send & Receive mail"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	gnome_dialog_set_sensitive(gd, 0, FALSE);

	frame= (GtkFrame *)gtk_frame_new(_("Receiving"));
	gtk_box_pack_start((GtkBox *)gd->vbox, (GtkWidget *)frame, TRUE, TRUE, 0);
	table = (GtkTable *)gtk_table_new(g_slist_length(sources), 3, FALSE);
	gtk_container_add((GtkContainer *)frame, (GtkWidget *)table);
	gtk_widget_show((GtkWidget *)frame);

	row = 0;
	while (sources) {
		MailConfigService *source = sources->data;

		info = g_malloc0(sizeof(*info));
		/* imap is handled differently */
		if (!strncmp(source->url, "imap:", 5))
			info->type = SEND_UPDATE;
		else
			info->type = SEND_RECEIVE;
		printf("adding source %s\n", source->url);

		label = (GtkLabel *)gtk_label_new(source->url);
		bar = (GtkProgressBar *)gtk_progress_bar_new();
		stop = (GtkButton *)gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);

		gtk_progress_set_show_text((GtkProgress *)bar, TRUE);
		if (info->type == SEND_UPDATE) {
			gtk_progress_set_format_string((GtkProgress *)bar, _("Updating ..."));
		} else {
			gtk_progress_set_format_string((GtkProgress *)bar, _("Waiting ..."));
		}

		gtk_table_attach(table, (GtkWidget *)label, 0, 1, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
		gtk_table_attach(table, (GtkWidget *)bar, 1, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
		gtk_table_attach(table, (GtkWidget *)stop, 2, 3, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);

		info->bar = bar;
		info->uri = g_strdup(source->url);
		info->keep = source->keep_on_server;
		info->cancel = camel_cancel_new();
		info->stop = stop;
		info->data = data;
		info->state = SEND_ACTIVE;
		data->active++;

		list = g_list_prepend(list, info);

		gtk_signal_connect((GtkObject *)stop, "clicked", receive_cancel, info);
		sources = sources->next;
		row++;
	}

	gtk_widget_show_all((GtkWidget *)table);

	if (outbox) {
		frame= (GtkFrame *)gtk_frame_new(_("Sending"));
		gtk_box_pack_start((GtkBox *)gd->vbox, (GtkWidget *)frame, TRUE, TRUE, 0);
		table = (GtkTable *)gtk_table_new(1, 3, FALSE);
		gtk_container_add((GtkContainer *)frame, (GtkWidget *)table);
		gtk_widget_show((GtkWidget *)frame);

		info = g_malloc0(sizeof(*info));
		info->type = SEND_SEND;
		printf("adding dest %s\n", destination);
	
		label = (GtkLabel *)gtk_label_new(destination);
		bar = (GtkProgressBar *)gtk_progress_bar_new();
		stop = (GtkButton *)gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
		
		gtk_progress_set_format_string((GtkProgress *)bar, _("Waiting ..."));
		gtk_progress_set_show_text((GtkProgress *)bar, TRUE);
		
		gtk_table_attach(table, (GtkWidget *)label, 0, 1, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
		gtk_table_attach(table, (GtkWidget *)bar, 1, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
		gtk_table_attach(table, (GtkWidget *)stop, 2, 3, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
	
		info->bar = bar;
		info->uri = g_strdup(destination);
		info->keep = FALSE;
		info->cancel = camel_cancel_new();
		info->stop = stop;
		info->data = data;
		info->state = SEND_ACTIVE;
		data->active++;
	
		list = g_list_prepend(list, info);
	
		gtk_signal_connect((GtkObject *)stop, "clicked", receive_cancel, info);
		gtk_widget_show_all((GtkWidget *)table);
	}

	gtk_widget_show((GtkWidget *)gd);

	gtk_signal_connect((GtkObject *)gd, "clicked", dialogue_clicked, data);

	data->infos = list;
	data->gd = gd;
	gtk_object_set_data_full((GtkObject *)gd, "info_data", data, free_info_data);

	return data;
}

static void
update_folders(char *uri, struct _folder_info *info, void *data)
{
	time_t now = *((time_t *)data);

	printf("checking update for folder: %s\n", info->uri);

	/* let it flow through to the folders every 10 seconds */
	/* we back off slowly as we progress */
	if (now > info->update+10+info->count*5) {
		printf("upating a folder: %s\n", info->uri);
		camel_folder_thaw(info->folder);
		camel_folder_freeze(info->folder);
		info->update = now;
		info->count++;
	}
}

/* for forwarding stuff to the gui thread */
struct _status_msg {
	struct _mail_msg msg;
	char *desc;
	int pc;
	struct _send_info *info;
};

static void
do_show_status(struct _mail_msg *mm)
{
	struct _status_msg *m = (struct _status_msg *)mm;

	gtk_progress_set_percentage((GtkProgress *)m->info->bar, (gfloat)(m->pc/100.0));
	gtk_progress_set_format_string((GtkProgress *)m->info->bar, m->desc);
}

static void
do_free_status(struct _mail_msg *mm)
{
	struct _status_msg *m = (struct _status_msg *)mm;

	g_free(m->desc);
}

struct _mail_msg_op status_op = {
	NULL,
	do_show_status,
	NULL,
	do_free_status,
};

static void
receive_status (CamelFilterDriver *driver, enum camel_filter_status_t status, int pc, const char *desc, void *data)
{
	struct _send_info *info = data;
	time_t now;
	struct _status_msg *m;

	/* only update every second */
	now = time(0);
	if (now <= info->update)
		return;

	info->update = now;

	/* let it flow through to the folder, every now and then too? */
	g_hash_table_foreach(info->data->folders, (GHFunc)update_folders, &now);

	if (info->data->inbox && now > info->data->inbox_update+20) {
		printf("updating inbox too\n");
		/* this doesn't seem to work right :( */
		camel_folder_thaw(info->data->inbox);
		camel_folder_freeze(info->data->inbox);
		info->data->inbox_update = now;
	}

	/* we just pile them onto the port, assuming it can handle it.
	   We could also have a receiver port and see if they've been processed
	   yet, so if this is necessary its not too hard to add */
	/* the mail_gui_port receiver will free everything for us */
	switch (status) {
	case FILTER_STATUS_START:
	case FILTER_STATUS_END:
		m = mail_msg_new(&status_op, NULL, sizeof(*m));
		m->desc = g_strdup(desc);
		m->pc = pc;
		m->info = info;
		e_msgport_put(mail_gui_port, (EMsg *)m);
		break;
	default:
		break;
	}
}

/* when receive/send is complete */
static void
receive_done (char *uri, void *data)
{
	struct _send_info *info = data;

	gtk_progress_set_percentage((GtkProgress *)info->bar, (gfloat)1.0);

	switch(info->state) {
	case SEND_CANCELLED:
		gtk_progress_set_format_string((GtkProgress *)info->bar, _("Cancelled."));
		break;
	default:
		info->state = SEND_COMPLETE;
		gtk_progress_set_format_string((GtkProgress *)info->bar, _("Complete."));
	}

	gtk_widget_set_sensitive((GtkWidget *)info->stop, FALSE);

	info->data->active--;
	if (info->data->active == 0) {
		gnome_dialog_set_sensitive(info->data->gd, 0, TRUE);
		gnome_dialog_set_sensitive(info->data->gd, 1, FALSE);
	}
}

/* same for updating */
static void
receive_update_done(CamelStore *store, void *data)
{
	receive_done("", data);
}

/* although we dont do anythign smart here yet, there is no need for this interface to
   be available to anyone else.
   This can also be used to hook into which folders are being updated, and occasionally
   let them refresh */
static CamelFolder *
receive_get_folder(CamelFilterDriver *d, const char *uri, void *data, CamelException *ex)
{
	struct _send_info *info = data;
	CamelFolder *folder;
	struct _folder_info *oldinfo;
	char *oldkey;

	g_mutex_lock(info->data->lock);
	folder = g_hash_table_lookup(info->data->folders, uri);
	g_mutex_unlock(info->data->lock);
	if (folder) {
		camel_object_ref((CamelObject *)folder);
		return folder;
	}
	folder = mail_tool_uri_to_folder(uri, ex);
	if (!folder)
		return NULL;

	/* we recheck that the folder hasn't snuck in while we were loading it ... */
	/* and we assume the newer one is the same, but unref the old one anyway */
	g_mutex_lock(info->data->lock);
	if (g_hash_table_lookup_extended(info->data->folders, uri, (void **)&oldkey, (void **)&oldinfo)) {
		camel_object_unref((CamelObject *)oldinfo->folder);
		oldinfo->folder = folder;
	} else {
		oldinfo = g_malloc0(sizeof(*oldinfo));
		oldinfo->folder = folder;
		oldinfo->uri = g_strdup(uri);
		g_hash_table_insert(info->data->folders, oldinfo->uri, oldinfo);
	}
	g_mutex_unlock(info->data->lock);

	return folder;
}

void mail_send_receive(void)
{
	GSList *sources;
	GList *scan;
	FilterContext *fc;
	struct _send_data *data;
	extern CamelFolder *outbox_folder;
	const MailConfigAccount *account;
	CamelStore *store;
	CamelException *ex;

	sources = mail_config_get_sources();
	if (!sources)
		return;
	account = mail_config_get_default_account();
	if (!account || !account->transport)
		return;

	fc = load_context();

	/* what to do about pop before smtp ?
	   Well, probably hook into receive_done or receive_status on
	   the right pop account, and when it is, then kick off the
	   smtp one. */
	data = build_dialogue(sources, outbox_folder, account->transport->url);
	scan = data->infos;
	while (scan) {
		struct _send_info *info = scan->data;

		switch(info->type) {
		case SEND_RECEIVE:
			mail_fetch_mail(info->uri, info->keep,
					fc, FILTER_SOURCE_INCOMING,
					info->cancel,
					receive_get_folder, info,
					receive_status, info,
					receive_done, info);
			break;
		case SEND_SEND:
			/* todo, store the folder in info? */
			mail_send_queue(outbox_folder, info->uri,
					fc, FILTER_SOURCE_OUTGOING,
					info->cancel,
					receive_get_folder, info,
					receive_status, info,
					receive_done, info);
			break;
		case SEND_UPDATE:
			/* FIXME: error reporting? */
			ex = camel_exception_new();
			store = camel_session_get_store(session, info->uri, ex);
			if (store) {
				EvolutionStorage *storage = mail_lookup_storage(store);
				if (storage) {
					mail_update_subfolders(store, storage, receive_update_done, info);
					gtk_object_unref((GtkObject *)storage);
				} else {
					receive_done("", info);
				}
				camel_object_unref((CamelObject *)store);
			} else {
				receive_done("", info);
			}
			camel_exception_free(ex);
			break;
		}
		scan = scan->next;
	}

	gtk_object_unref((GtkObject *)fc);
}

void mail_filter_on_demand(CamelFolder *folder, GPtrArray *uids)
{
	FilterContext *fc;

	fc = load_context();
	mail_filter_folder(folder, uids, fc, FILTER_SOURCE_INCOMING, NULL);
	gtk_object_unref((GtkObject *)fc);
}

