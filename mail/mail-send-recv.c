
#include <stdio.h>
#include <string.h>

#include "filter/filter-context.h"
#include "filter/filter-filter.h"
#include "camel/camel-filter-driver.h"
#include "camel/camel-folder.h"
#include "camel/camel-session.h"
#include "camel/camel-uid-cache.h"

#include "mail-mt.h"
#include "mail-config.h"

/* for the dialogue stuff */
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-dialog.h>

/* send/receive email */

extern char *evolution_dir;
extern CamelFolder *mail_tool_filter_get_folder_func (CamelFilterDriver *d, const char *uri, void *data);

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

/* used for both just filtering a folder + uid's, and for filtering a whole folder */
struct _fetch_mail_msg {
	struct _mail_msg msg;

	char *source_uri;
	CamelFolder *source_folder;
	GPtrArray *source_uids;
	CamelCancel *cancel;
	CamelFolder *destination;
	CamelFilterDriver *driver;
	int keep;

	void (*done)(char *source, void *data);
	void *data;
};

/* filter a folder, or a subset thereof, uses source_folder/source_uids */
static void
fetch_mail_filter_folder(struct _mail_msg *mm)
{
	struct _fetch_mail_msg *m = (struct _fetch_mail_msg *)mm;
	CamelFolder *folder;
	CamelUIDCache *cache = NULL;
	GPtrArray *uids, *folder_uids = NULL, *cache_uids = NULL;

	if (m->source_folder)
		folder = m->source_folder;
	else
		folder = m->source_folder = mail_tool_get_inbox (m->source_uri, &mm->ex);

	if (folder == NULL || camel_folder_get_message_count (folder) == 0) {
		return;
	}

	camel_folder_freeze(folder);

	if (m->source_uids)
		uids = m->source_uids;
	else
		uids = camel_folder_get_uids (folder);

	if (m->keep) {
		char *cachename = mail_config_folder_to_cachename (folder, "cache-");
		
		cache = camel_uid_cache_new (cachename);
		if (cache) {
			cache_uids = camel_uid_cache_get_new_uids (cache, uids);
			uids = cache_uids;
		}
		
		g_free (cachename);
	}

	camel_filter_driver_filter_folder(m->driver, folder, FILTER_SOURCE_INCOMING, uids, !m->keep, &mm->ex);

	if (cache) {
		if (!camel_exception_is_set (&mm->ex))
			camel_uid_cache_save (cache);
		camel_uid_cache_destroy (cache);
	}

	if (folder_uids)
		camel_folder_free_uids(folder, folder_uids);
	if (cache_uids)
		camel_uid_cache_free_uids (cache_uids);

	/* sync and expunge */
	camel_folder_sync (folder, TRUE, &mm->ex);
	camel_folder_thaw(folder);
}

static void
fetch_mail_fetch(struct _mail_msg *mm)
{
	struct _fetch_mail_msg *m = (struct _fetch_mail_msg *)mm;
	
	if (m->cancel)
		camel_cancel_register(m->cancel);

	if (m->destination == NULL) {
		if ( (m->destination = mail_tool_get_local_inbox(&mm->ex)) == NULL) {
			if (m->cancel)
				camel_cancel_unregister(m->cancel);
			return;
		}
	}

	camel_folder_freeze (m->destination);
	camel_filter_driver_set_default_folder(m->driver, m->destination);

	/* FIXME: this should support keep_on_server too, which would then perform a spool
	   access thingy, right?  problem is matching raw messages to uid's etc. */
	if (!strncmp (m->source_uri, "mbox:", 5)) {
		char *path = mail_tool_do_movemail (m->source_uri, &mm->ex);
		
		if (path && !camel_exception_is_set (&mm->ex)) {
			camel_filter_driver_filter_mbox(m->driver, path, FILTER_SOURCE_INCOMING, &mm->ex);
			
			/* ok?  zap the output file */
			if (!camel_exception_is_set (&mm->ex)) {
				unlink (path);
			}
		}
		g_free (path);
	} else {
		fetch_mail_filter_folder(mm);
	}

	camel_folder_thaw (m->destination);

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
	int i;

	g_free(m->source_uri);
	if (m->source_folder)
		camel_object_unref((CamelObject *)m->source_folder);
	if (m->source_uids) {
		for (i=0;i<m->source_uids->len;i++)
			g_free(m->source_uids->pdata[i]);
		g_ptr_array_free(m->source_uids, TRUE);
	}
	camel_cancel_unref(m->cancel);
	if (m->destination)
		camel_object_unref((CamelObject *)m->destination);
	camel_object_unref((CamelObject *)m->driver);
}

static struct _mail_msg_op fetch_mail_op = {
	NULL,			/* we do our own progress reporting */
	fetch_mail_fetch,
	fetch_mail_fetched,
	fetch_mail_free,
};

/* ouch, a 'do everything' interface ... */
void mail_filter_mail(const char *source, int keep,
		      FilterContext *fc, const char *type,
		      CamelCancel *cancel,
		      CamelFilterGetFolderFunc get_folder, void *get_data,
		      CamelFilterStatusFunc *status, void *status_data,
		      void (*done)(char *source, void *data), void *data)
{
	struct _fetch_mail_msg *m;

	m = mail_msg_new(&fetch_mail_op, NULL, sizeof(*m));
	m->source_uri = g_strdup(source);
	m->keep = keep;
	m->cancel = cancel;
	camel_cancel_ref(cancel);
	m->done = done;
	m->data = data;

	m->driver = camel_filter_driver_new(get_folder, get_data);
	setup_filter_driver(m->driver, fc, type);
	if (status)
		camel_filter_driver_set_status_func(m->driver, status, status_data);

	e_thread_put(mail_thread_new, (EMsg *)m);
}

/* this is not used yet, will be used to keep track of which folders were
   filtering to, and to let them refresh once in a while */
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

struct _send_info {
	CamelCancel *cancel;
	char *uri;
	int keep;
	int cancelled;
	GtkProgressBar *bar;
	GtkButton *stop;
	time_t update;
	struct _send_data *data;
};

static void
receive_cancel(GtkButton *button, struct _send_info *info)
{
	camel_cancel_cancel(info->cancel);
	gtk_progress_set_format_string((GtkProgress *)info->bar, _("Cancelling ..."));
	info->cancelled = TRUE;
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

static struct _send_data *build_dialogue(GSList *sources)
{
	GnomeDialog *gd;
	GtkFrame *frame;
	GtkTable *table;
	int row;
	GList *list = NULL;
	struct _send_data *data;

	data = g_malloc0(sizeof(*data));
	data->lock = g_mutex_new();
	data->folders = g_hash_table_new(g_str_hash, g_str_equal);
	data->inbox = mail_tool_get_local_inbox(NULL);

	gd = (GnomeDialog *)gnome_dialog_new(_("Send & Receive mail"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	gnome_dialog_set_sensitive(gd, 0, FALSE);

	frame= (GtkFrame *)gtk_frame_new(_("Receive list"));
	gtk_box_pack_start((GtkBox *)gd->vbox, (GtkWidget *)frame, TRUE, TRUE, 0);
	table = (GtkTable *)gtk_table_new(g_slist_length(sources), 3, FALSE);
	gtk_container_add((GtkContainer *)frame, (GtkWidget *)table);
	gtk_widget_show((GtkWidget *)frame);

	row = 0;
	while (sources) {
		MailConfigService *source = sources->data;
		GtkLabel *label;
		GtkProgressBar *bar;
		GtkButton *stop;
		struct _send_info *info;

		/* imap is handled differently */
		if (!strncmp(source->url, "imap:", 5)) {
			sources = sources->next;
			continue;
		}

		info = g_malloc0(sizeof(*info));
		printf("adding source %s\n", source->url);

		label = (GtkLabel *)gtk_label_new(source->url);
		bar = (GtkProgressBar *)gtk_progress_bar_new();
		stop = (GtkButton *)gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);

		gtk_progress_set_format_string((GtkProgress *)bar, _("Waiting ..."));
		gtk_progress_set_show_text((GtkProgress *)bar, TRUE);

		gtk_table_attach(table, (GtkWidget *)label, 0, 1, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
		gtk_table_attach(table, (GtkWidget *)bar, 1, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
		gtk_table_attach(table, (GtkWidget *)stop, 2, 3, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);

		info->bar = bar;
		info->uri = g_strdup(source->url);
		info->keep = source->keep_on_server;
		info->cancel = camel_cancel_new();
		info->stop = stop;
		info->data = data;
		data->active++;

		list = g_list_prepend(list, info);

		gtk_signal_connect((GtkObject *)stop, "clicked", receive_cancel, info);
		sources = sources->next;
		row++;
	}

	gtk_widget_show_all((GtkWidget *)table);
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

static void
receive_done (char *uri, void *data)
{
	struct _send_info *info = data;

	if (info->cancelled)
		gtk_progress_set_format_string((GtkProgress *)info->bar, _("Cancelled."));
	else
		gtk_progress_set_format_string((GtkProgress *)info->bar, _("Complete."));

	gtk_widget_set_sensitive((GtkWidget *)info->stop, FALSE);

	info->data->active--;
	if (info->data->active == 0) {
		gnome_dialog_set_sensitive(info->data->gd, 0, TRUE);
		gnome_dialog_set_sensitive(info->data->gd, 1, FALSE);
	}
}

/* although we dont do anythign smart here yet, there is no need for this interface to
   be available to anyone else.
   This can also be used to hook into which folders are being updated, and occasionally
   let them refresh */
static CamelFolder *
receive_get_folder(CamelFilterDriver *d, const char *uri, void *data)
{
	struct _send_info *info = data;
	CamelFolder *mail_tool_uri_to_folder_noex (const char *uri);
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
	folder = mail_tool_uri_to_folder_noex(uri);
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

void mail_receive(void)
{
	GSList *sources;
	GList *scan;
	FilterContext *fc;
	struct _send_data *data;

	sources = mail_config_get_sources ();
	if (!sources)
		return;

	fc = load_context();

	data = build_dialogue(sources);
	scan = data->infos;
	while (scan) {
		struct _send_info *info = scan->data;
		mail_filter_mail(info->uri, info->keep,
				 fc, FILTER_SOURCE_INCOMING,
				 info->cancel,
				 receive_get_folder, info,
				 receive_status, info,
				 receive_done, info);
		scan = scan->next;
	}

	gtk_object_unref((GtkObject *)fc);
}

