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
#include "camel/camel-session.h"

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
	camel_folder_thaw (info->folder);	
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
	if (data->inbox) {
		camel_folder_thaw (data->inbox);		
		camel_object_unref((CamelObject *)data->inbox);
	}
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
	camel_folder_freeze (data->inbox);

	gd = (GnomeDialog *)gnome_dialog_new(_("Send & Receive mail"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	gnome_dialog_set_sensitive(gd, 0, FALSE);
	gnome_window_icon_set_from_file((GtkWindow *)gd, EVOLUTION_DATADIR "images/evolution/evolution-inbox.png");

	frame= (GtkFrame *)gtk_frame_new(_("Receiving"));
	gtk_box_pack_start((GtkBox *)gd->vbox, (GtkWidget *)frame, TRUE, TRUE, 0);
	table = (GtkTable *)gtk_table_new(g_slist_length(sources), 3, FALSE);
	gtk_container_add((GtkContainer *)frame, (GtkWidget *)table);
	gtk_widget_show((GtkWidget *)frame);

	row = 0;
	while (sources) {
		MailConfigService *source = sources->data;
		
		if (!source->url) {
			sources = sources->next;
			continue;
		}
		
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
	char *out, *p, *o, c;

	out = alloca(strlen(m->desc)*2+1);
	o = out;
	p = m->desc;
	while ((c = *p++)) {
		if (c=='%')
			*o++ = '%';
		*o++ = c;
	}
	*o = 0;
	gtk_progress_set_percentage((GtkProgress *)m->info->bar, (gfloat)(m->pc/100.0));
	gtk_progress_set_format_string((GtkProgress *)m->info->bar, out);
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
	case CAMEL_FILTER_STATUS_START:
	case CAMEL_FILTER_STATUS_END:
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
		camel_folder_freeze (folder);		
		oldinfo = g_malloc0(sizeof(*oldinfo));
		oldinfo->folder = folder;
		oldinfo->uri = g_strdup(uri);
		g_hash_table_insert(info->data->folders, oldinfo->uri, oldinfo);
	}
	g_mutex_unlock(info->data->lock);

	camel_object_ref((CamelObject *)folder);
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

