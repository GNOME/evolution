/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <NotZed@ximian.com>
 *
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

#include "config.h"
#include "mail-send-recv.h"

#include <stdio.h>
#include <string.h>

#include "filter/filter-context.h"
#include "filter/filter-filter.h"
#include "camel/camel-filter-driver.h"
#include "camel/camel-folder.h"
#include "camel/camel-operation.h"

#include "evolution-storage.h"

#include "mail.h"
#include "mail-mt.h"
#include "mail-config.h"
#include "mail-session.h"
#include "mail-tools.h"
#include "mail-ops.h"

/* for the dialogue stuff */
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-window-icon.h>

#define d(x)

/* ms between status updates to the gui */
#define STATUS_TIMEOUT (250)

/* send/receive email */

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

	GnomeDialog *gd;
	int cancelled;

	CamelFolder *inbox;	/* since we're never asked to update this one, do it ourselves */
	time_t inbox_update;

	GMutex *lock;
	GHashTable *folders;

	GHashTable *active;	/* send_info's by uri */
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
	CamelOperation *cancel;
	char *uri;
	int keep;
	send_state_t state;
	GtkProgressBar *bar;
	GtkButton *stop;

	int timeout_id;
	char *what;
	int pc;

	/*time_t update;*/
	struct _send_data *data;
};

static struct _send_data *send_data = NULL;

static struct _send_data *setup_send_data(void)
{
	struct _send_data *data;
	
	if (send_data == NULL) {
		send_data = data = g_malloc0(sizeof(*data));
		data->lock = g_mutex_new();
		data->folders = g_hash_table_new(g_str_hash, g_str_equal);
		data->inbox = mail_tool_get_local_inbox(NULL);
		data->active = g_hash_table_new(g_str_hash, g_str_equal);
	}
	return send_data;
}

static void
receive_cancel(GtkButton *button, struct _send_info *info)
{
	if (info->state == SEND_ACTIVE) {
		camel_operation_cancel(info->cancel);
		if (info->bar)
			gtk_progress_set_format_string((GtkProgress *)info->bar, _("Cancelling ..."));
		info->state = SEND_CANCELLED;
	}
	if (info->stop)
		gtk_widget_set_sensitive((GtkWidget *)info->stop, FALSE);
}

static void
free_folder_info(void *key, struct _folder_info *info, void *data)
{
	/*camel_folder_thaw (info->folder);	*/
	camel_object_unref((CamelObject *)info->folder);
	g_free(info->uri);
}

static void free_send_info(void *key, struct _send_info *info, void *data)
{
	d(printf("Freeing send info %p\n", info));
	g_free(info->uri);
	camel_operation_unref(info->cancel);
	if (info->timeout_id != 0)
		gtk_timeout_remove(info->timeout_id);
	g_free(info);
}

static void
free_send_data(void)
{
	struct _send_data *data = send_data;

	g_assert(g_hash_table_size(data->active) == 0);

	g_list_free(data->infos);
	g_hash_table_foreach(data->active, (GHFunc)free_send_info, NULL);
	g_hash_table_destroy(data->active);
	g_hash_table_foreach(data->folders, (GHFunc)free_folder_info, NULL);
	g_hash_table_destroy(data->folders);
	g_mutex_free(data->lock);
	if (data->inbox) {
		/*camel_folder_thaw (data->inbox);		*/
		camel_object_unref((CamelObject *)data->inbox);
	}
	g_free(data);
	send_data = NULL;
}


static void cancel_send_info(void *key, struct _send_info *info, void *data)
{
	receive_cancel(info->stop, info);
}

static void hide_send_info(void *key, struct _send_info *info, void *data)
{
	info->stop = NULL;
	info->bar = NULL;
}

static void
dialogue_clicked(GnomeDialog *gd, int button, struct _send_data *data)
{
	switch(button) {
	case 0:
		d(printf("cancelled whole thing\n"));
		if (!data->cancelled) {
			data->cancelled = TRUE;
			g_hash_table_foreach(data->active, (GHFunc)cancel_send_info, NULL);
		}
		gnome_dialog_set_sensitive(gd, 0, FALSE);
		break;
	case -1:		/* dialogue vanished, so make out its just hidden */
		d(printf("hiding dialogue\n"));
		g_hash_table_foreach(data->active, (GHFunc)hide_send_info, NULL);
		break;
	}
}

static void operation_status(CamelOperation *op, const char *what, int pc, void *data);
static int operation_status_timeout(void *data);

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

	data = setup_send_data();

	gd = (GnomeDialog *)gnome_dialog_new(_("Send & Receive mail"), GNOME_STOCK_BUTTON_CANCEL, NULL);
	gnome_window_icon_set_from_file((GtkWindow *)gd, EVOLUTION_DATADIR "images/evolution/evolution-inbox.png");

	frame= (GtkFrame *)gtk_frame_new(_("Receiving"));
	gtk_box_pack_start((GtkBox *)gd->vbox, (GtkWidget *)frame, TRUE, TRUE, 0);
	table = (GtkTable *)gtk_table_new(g_slist_length(sources), 3, FALSE);
	gtk_container_add((GtkContainer *)frame, (GtkWidget *)table);
	gtk_widget_show((GtkWidget *)frame);

	row = 0;
	while (sources) {
		MailConfigService *source = sources->data;
		
		if (!source->url
		    || !source->enabled) {
			sources = sources->next;
			continue;
		}

		/* see if we have an outstanding download active */
		info = g_hash_table_lookup(data->active, source->url);
		if (info == NULL) {
			info = g_malloc0(sizeof(*info));
			/* imap is handled differently */
			if (!strncmp(source->url, "imap:", 5))
				info->type = SEND_UPDATE;
			else
				info->type = SEND_RECEIVE;
			d(printf("adding source %s\n", source->url));

			info->uri = g_strdup(source->url);
			info->keep = source->keep_on_server;
			info->cancel = camel_operation_new(operation_status, info);
			info->state = SEND_ACTIVE;
			info->timeout_id = gtk_timeout_add(STATUS_TIMEOUT, operation_status_timeout, info);

			g_hash_table_insert(data->active, info->uri, info);
			list = g_list_prepend(list, info);
		} else if (info->bar != NULL) {
			/* incase we get the same source pop up again */
			sources = sources->next;
			continue;
		} else if (info->timeout_id == 0)
			info->timeout_id = gtk_timeout_add(STATUS_TIMEOUT, operation_status_timeout, info);

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
		info->stop = stop;
		info->data = data;

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

		info = g_hash_table_lookup(data->active, destination);
		if (info == NULL) {
			info = g_malloc0(sizeof(*info));
			info->type = SEND_SEND;
			d(printf("adding dest %s\n", destination));

			info->uri = g_strdup(destination);
			info->keep = FALSE;
			info->cancel = camel_operation_new(operation_status, info);
			info->state = SEND_ACTIVE;
			info->timeout_id = gtk_timeout_add(STATUS_TIMEOUT, operation_status_timeout, info);

			g_hash_table_insert(data->active, info->uri, info);
			list = g_list_prepend(list, info);
		} else if (info->timeout_id == 0)
			info->timeout_id = gtk_timeout_add(STATUS_TIMEOUT, operation_status_timeout, info);

		label = (GtkLabel *)gtk_label_new(destination);
		bar = (GtkProgressBar *)gtk_progress_bar_new();
		stop = (GtkButton *)gnome_stock_button(GNOME_STOCK_BUTTON_CANCEL);
		
		gtk_progress_set_format_string((GtkProgress *)bar, _("Waiting ..."));
		gtk_progress_set_show_text((GtkProgress *)bar, TRUE);
		
		gtk_table_attach(table, (GtkWidget *)label, 0, 1, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
		gtk_table_attach(table, (GtkWidget *)bar, 1, 2, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
		gtk_table_attach(table, (GtkWidget *)stop, 2, 3, row, row+1, GTK_EXPAND|GTK_FILL, 0, 3, 1);
	
		info->bar = bar;
		info->stop = stop;
		info->data = data;
		
		gtk_signal_connect((GtkObject *)stop, "clicked", receive_cancel, info);
		gtk_widget_show_all((GtkWidget *)table);
	}

	gtk_widget_show((GtkWidget *)gd);

	gtk_signal_connect((GtkObject *)gd, "clicked", dialogue_clicked, data);

	data->infos = list;
	data->gd = gd;

	return data;
}

static void
update_folders(char *uri, struct _folder_info *info, void *data)
{
	time_t now = *((time_t *)data);

	d(printf("checking update for folder: %s\n", info->uri));

	/* let it flow through to the folders every 10 seconds */
	/* we back off slowly as we progress */
	if (now > info->update+10+info->count*5) {
		d(printf("upating a folder: %s\n", info->uri));
		/*camel_folder_thaw(info->folder);
		  camel_folder_freeze(info->folder);*/
		info->update = now;
		info->count++;
	}
}

static void set_send_status(struct _send_info *info, const char *desc, int pc)
{
	const char *p;
	char *out, *o, c;

	out = alloca(strlen(desc)*2+1);
	o = out;
	p = desc;
	while ((c = *p++)) {
		if (c=='%')
			*o++ = '%';
		*o++ = c;
	}
	*o = 0;
	
	/* FIXME: LOCK */
	g_free(info->what);
	info->what = g_strdup(out);
	info->pc = pc;
}

static void
receive_status (CamelFilterDriver *driver, enum camel_filter_status_t status, int pc, const char *desc, void *data)
{
	struct _send_info *info = data;
	time_t now;

	/* let it flow through to the folder, every now and then too? */
	g_hash_table_foreach(info->data->folders, (GHFunc)update_folders, &now);

	if (info->data->inbox && now > info->data->inbox_update+20) {
		d(printf("updating inbox too\n"));
		/* this doesn't seem to work right :( */
		/*camel_folder_thaw(info->data->inbox);
		  camel_folder_freeze(info->data->inbox);*/
		info->data->inbox_update = now;
	}

	/* we just pile them onto the port, assuming it can handle it.
	   We could also have a receiver port and see if they've been processed
	   yet, so if this is necessary its not too hard to add */
	/* the mail_gui_port receiver will free everything for us */
	switch (status) {
	case CAMEL_FILTER_STATUS_START:
	case CAMEL_FILTER_STATUS_END:
		set_send_status(info, desc, pc);
		break;
	default:
		break;
	}
}

static int operation_status_timeout(void *data)
{
	struct _send_info *info = data;

	if (info->bar) {
		gtk_progress_set_percentage((GtkProgress *)info->bar, (gfloat)(info->pc/100.0));
		gtk_progress_set_format_string((GtkProgress *)info->bar, info->what);

		return TRUE;
	}

	return FALSE;
}

/* for camel operation status */
static void operation_status(CamelOperation *op, const char *what, int pc, void *data)
{
	struct _send_info *info = data;

	/*printf("Operation '%s', percent %d\n");*/
	switch (pc) {
	case CAMEL_OPERATION_START:
		pc = 0;
		break;
	case CAMEL_OPERATION_END:
		pc = 100;
		break;
	}

	set_send_status(info, what, pc);
}

/* when receive/send is complete */
static void
receive_done (char *uri, void *data)
{
	struct _send_info *info = data;

	if (info->bar) {
		gtk_progress_set_percentage((GtkProgress *)info->bar, (gfloat)1.0);

		switch(info->state) {
		case SEND_CANCELLED:
			gtk_progress_set_format_string((GtkProgress *)info->bar, _("Cancelled."));
			break;
		default:
			info->state = SEND_COMPLETE;
			gtk_progress_set_format_string((GtkProgress *)info->bar, _("Complete."));
		}
	}

	if (info->stop)
		gtk_widget_set_sensitive((GtkWidget *)info->stop, FALSE);

	/* remove/free this active download */
	d(printf("%s: freeing info %p\n", __FUNCTION__, info));
	g_hash_table_remove(info->data->active, info->uri);
	info->data->infos = g_list_remove(info->data->infos, info);
	g_free(info->uri);
	camel_operation_unref(info->cancel);
	if (info->timeout_id)
		gtk_timeout_remove(info->timeout_id);

	if (g_hash_table_size(info->data->active) == 0) {
		if (info->data->gd)
			gnome_dialog_close(info->data->gd);
		free_send_data();
	}

	g_free(info);
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
	oldinfo = g_hash_table_lookup(info->data->folders, uri);
	g_mutex_unlock(info->data->lock);
	if (oldinfo) {
		camel_object_ref((CamelObject *)oldinfo->folder);
		return oldinfo->folder;
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
		/*camel_folder_freeze (folder);		*/
		oldinfo = g_malloc0(sizeof(*oldinfo));
		oldinfo->folder = folder;
		oldinfo->uri = g_strdup(uri);
		g_hash_table_insert(info->data->folders, oldinfo->uri, oldinfo);
	}
	
	camel_object_ref (CAMEL_OBJECT (folder));
	
	g_mutex_unlock(info->data->lock);
	
	return folder;
}

static void
receive_update_got_store(char *uri, CamelStore *store, void *data)
{
	struct _send_info *info = data;

	if (store) {
		EvolutionStorage *storage = mail_lookup_storage(store);
		if (storage) {
			mail_update_subfolders(store, storage, receive_update_done, info);
			gtk_object_unref((GtkObject *)storage);
		} else {
			receive_done("", info);
		}
	} else {
		receive_done("", info);
	}
}

void mail_send_receive(void)
{
	GSList *sources;
	GList *scan;
	FilterContext *fc;
	static GtkWidget *gd = NULL;
	struct _send_data *data;
	extern CamelFolder *outbox_folder;
	const MailConfigAccount *account;

	if (gd != NULL) {
		g_assert(GTK_WIDGET_REALIZED(gd));
		gdk_window_show(gd->window);
		gdk_window_raise(gd->window);
		return;
	}

	sources = mail_config_get_sources();
	if (!sources)
		return;
	account = mail_config_get_default_account();
	if (!account || !account->transport)
		return;
	
	fc = mail_load_filter_context();
		
	/* what to do about pop before smtp ?
	   Well, probably hook into receive_done or receive_status on
	   the right pop account, and when it is, then kick off the
	   smtp one. */
	data = build_dialogue(sources, outbox_folder, account->transport->url);
	scan = data->infos;
	gd = GTK_WIDGET(data->gd);
	gtk_signal_connect((GtkObject *)gd, "destroy", gtk_widget_destroyed, &gd);
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
			mail_get_store(info->uri, receive_update_got_store, info);
			break;
		}
		scan = scan->next;
	}
	
	gtk_object_unref((GtkObject *)fc);
}

struct _auto_data {
	char *uri;
	int keep;		/* keep on server flag */
	int period;		/* in seconds */
	int timeout_id;
};

static GHashTable *auto_active;

static gboolean
auto_timeout(void *data)
{
	struct _auto_data *info = data;

	mail_receive_uri(info->uri, info->keep);

	return TRUE;
}

static void auto_setup_set(void *key, struct _auto_data *info, GHashTable *set)
{
	g_hash_table_insert(set, info->uri, info);
}

static void auto_clean_set(void *key, struct _auto_data *info, GHashTable *set)
{
	d(printf("removing auto-check for %s %p\n", info->uri, info));
	g_hash_table_remove(set, info->uri);
	gtk_timeout_remove(info->timeout_id);
	g_free(info->uri);
	g_free(info);
}

/* call to setup initial, and after changes are made to the config */
/* FIXME: Need a cleanup funciton for when object is deactivated */
void
mail_autoreceive_setup(void)
{
	GSList *sources;
	GHashTable *set_hash;

	sources = mail_config_get_sources();
	if (!sources)
		return;

	if (auto_active == NULL)
		auto_active = g_hash_table_new(g_str_hash, g_str_equal);

	set_hash = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_foreach(auto_active, (GHFunc)auto_setup_set, set_hash);

	while (sources) {
		MailConfigService *source = sources->data;
		if (source->url && source->auto_check && source->enabled) {
			struct _auto_data *info;

			d(printf("setting up auto-receive mail for : %s\n", source->url));

			g_hash_table_remove(set_hash, source->url);
			info = g_hash_table_lookup(auto_active, source->url);
			if (info) {
				info->keep = source->keep_on_server;
				if (info->period != source->auto_check_time*60) {
					info->period = source->auto_check_time*60;
					gtk_timeout_remove(info->timeout_id);
					info->timeout_id = gtk_timeout_add(info->period*1000, auto_timeout, info);
				}
			} else {
				info = g_malloc0(sizeof(*info));
				info->uri = g_strdup(source->url);
				info->keep = source->keep_on_server;
				info->period = source->auto_check_time*60;
				info->timeout_id = gtk_timeout_add(info->period*1000, auto_timeout, info);
				g_hash_table_insert(auto_active, info->uri, info);
				/* If we do this at startup, it can cause the logon dialogue to be hidden,
				   so lets not */
				/*mail_receive_uri(source->url, source->keep_on_server);*/
			}
		}

		sources = sources->next;
	}

	g_hash_table_foreach(set_hash, (GHFunc)auto_clean_set, auto_active);
	g_hash_table_destroy(set_hash);
}

/* we setup the download info's in a hashtable, if we later need to build the gui, we insert
   them in to add them. */
void mail_receive_uri(const char *uri, int keep)
{
	FilterContext *fc;
	struct _send_info *info;
	struct _send_data *data;
	extern CamelFolder *outbox_folder;

	data = setup_send_data();
	info = g_hash_table_lookup(data->active, uri);
	if (info != NULL) {
		d(printf("download of %s still in progress\n", uri));
		return;
	}

	d(printf("starting non-interactive download of '%s'\n", uri));

	info = g_malloc0(sizeof(*info));
	/* imap is handled differently */
	if (!strncmp(uri, "imap:", 5))
		info->type = SEND_UPDATE;
	else
		info->type = SEND_RECEIVE;

	info->bar = NULL;
	info->uri = g_strdup(uri);
	info->keep = keep;
	info->cancel = camel_operation_new(operation_status, info);
	info->stop = NULL;
	info->data = data;
	info->state = SEND_ACTIVE;
	info->timeout_id = 0;

	d(printf("Adding new info %p\n", info));

	g_hash_table_insert(data->active, info->uri, info);

	fc = mail_load_filter_context();
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
		mail_get_store(info->uri, receive_update_got_store, info);
		break;
	}
	gtk_object_unref((GtkObject *)fc);
}

