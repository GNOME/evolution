/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-ops.c: callbacks for the mail toolbar/menus */

/* 
 * Author : 
 *  Dan Winship <danw@helixcode.com>
 *  Jeffrey Stedfast <fejj@helixcode.com>
 *  Peter Williams <peterw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include <gnome.h>
#include <ctype.h>
#include "mail.h"
#include "mail-threads.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "composer/e-msg-composer.h"
#include "folder-browser.h"

/* ** FETCH MAIL ********************************************************** */

typedef struct fetch_mail_input_s
{
	gchar *source_url;
	gboolean keep_on_server;
	CamelFolder *destination;
	gpointer hook_func;
	gpointer hook_data;
}
fetch_mail_input_t;

typedef struct fetch_mail_data_s {
	gboolean empty;
} fetch_mail_data_t;

static gchar *
describe_fetch_mail (gpointer in_data, gboolean gerund)
{
	fetch_mail_input_t *input = (fetch_mail_input_t *) in_data;
	char *name;
	
	/*source = camel_session_get_store (session, input->source_url, NULL);
	 *if (source) {
	 *	name = camel_service_get_name (CAMEL_SERVICE (source), FALSE);
	 *	camel_object_unref (CAMEL_OBJECT (source));
	 *} else
	 */
	name = input->source_url;
	
	if (gerund)
		return g_strdup_printf (_("Fetching email from %s"), name);
	else
		return g_strdup_printf (_("Fetch email from %s"), name);
}

static void
setup_fetch_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	fetch_mail_input_t *input = (fetch_mail_input_t *) in_data;
	fetch_mail_data_t *data = (fetch_mail_data_t *) op_data;
	
	data->empty = FALSE;
	if (input->destination)
		camel_object_ref (CAMEL_OBJECT (input->destination));
}

static FilterContext *
mail_load_evolution_rule_context ()
{
	gchar *userrules;
	gchar *systemrules;
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
mail_op_report_status (FilterDriver *driver, enum filter_status_t status, const char *desc, void *data)
{
	/* FIXME: make it work */
	switch (status) {
	case FILTER_STATUS_START:
		mail_op_set_message (desc);
		break;
	case FILTER_STATUS_END:
		break;
	case FILTER_STATUS_ACTION:
		break;
	default:
		break;
	}
}

static void
update_changed_folders (CamelStore *store, CamelFolderInfo *info,
			EvolutionStorage *storage, const char *path,
			CamelException *ex)
{
	CamelFolder *folder;
	char *name, *display;

	name = g_strdup_printf ("%s/%s", path, info->name);
	if (info->url) {
		if (info->unread_message_count > 0) {
			display = g_strdup_printf ("%s (%d)", info->name,
						   info->unread_message_count);
			evolution_storage_update_folder (storage, name,
							 display, TRUE);
			g_free (display);
		} else {
			evolution_storage_update_folder (storage, name,
							 info->name, FALSE);
		}

		/* This is a bit of a hack... if the store is already
		 * caching the folder, then we update it. Otherwise
		 * we don't.
		 */
		folder = CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS (store))->
			lookup_folder (store, info->full_name);		
		if (folder) {
			camel_folder_sync (folder, FALSE, ex);
			if (!camel_exception_is_set (ex))
				camel_folder_refresh_info (folder, ex);
			camel_object_unref (CAMEL_OBJECT (folder));
		}
	}
	if (!camel_exception_is_set (ex) && info->sibling) {
		update_changed_folders (store, info->sibling, storage,
					path, ex);
	}
	if (!camel_exception_is_set (ex) && info->child) {
		update_changed_folders (store, info->child, storage,
					name, ex);
	}
	g_free (name);
}

static void
do_fetch_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	fetch_mail_input_t *input = (fetch_mail_input_t *) in_data;
	fetch_mail_data_t *data = (fetch_mail_data_t *) op_data;
	FilterContext *fc;
	FilterDriver *filter;
	FILE *logfile = NULL;
	CamelFolder *folder;
	
	/* FIXME: This shouldn't be checking for "imap" specifically. */
	if (!strncmp (input->source_url, "imap:", 5)) {
		CamelStore *store;
		CamelFolderInfo *info;
		EvolutionStorage *storage;

		store = camel_session_get_store (session, input->source_url, ex);
		if (!store)
			return;
		storage = mail_lookup_storage (store);
		g_return_if_fail (storage != NULL);

		info = camel_store_get_folder_info (store, NULL, FALSE,
						    TRUE, TRUE, ex);
		if (!info) {
			camel_object_unref (CAMEL_OBJECT (store));
			gtk_object_unref (GTK_OBJECT (storage));
			return;
		}

		update_changed_folders (store, info, storage, "", ex);
		camel_store_free_folder_info (store, info);
		camel_object_unref (CAMEL_OBJECT (store));
		gtk_object_unref (GTK_OBJECT (storage));

		data->empty = FALSE;
		return;
	}
	
	if (input->destination == NULL) {
		input->destination = mail_tool_get_local_inbox (ex);
		
		if (input->destination == NULL)
			return;
	}
	
	/* setup filter driver */
	fc = mail_load_evolution_rule_context ();
	filter = filter_driver_new (fc, mail_tool_filter_get_folder_func, 0);
	filter_driver_set_default_folder (filter, input->destination);
	
	if (TRUE /* perform_logging */) {
		char *filename = g_strdup_printf ("%s/evolution-filter-log", evolution_dir);
		logfile = fopen (filename, "a+");
		g_free (filename);
	}
	
	filter_driver_set_logfile (filter, logfile);
	filter_driver_set_status_func (filter, mail_op_report_status, NULL);
	
	/* why on earth we 'up' a lock to get it, ... */
	mail_tool_camel_lock_up ();
	
	camel_folder_freeze (input->destination);
	
	if (!strncmp (input->source_url, "mbox:", 5)) {
		char *path = mail_tool_do_movemail (input->source_url, ex);
		
		if (path && !camel_exception_is_set (ex)) {
			filter_driver_filter_mbox (filter, path, FILTER_SOURCE_INCOMING, ex);
			
			/* ok?  zap the output file */
			if (!camel_exception_is_set (ex)) {
				unlink (path);
			}
		}
		g_free (path);
	} else {
		folder = mail_tool_get_inbox (input->source_url, ex);

		if (folder) {
			if (camel_folder_get_message_count (folder) > 0) {
				CamelUIDCache *cache = NULL;
				GPtrArray *uids;
				
				uids = camel_folder_get_uids (folder);
				if (input->keep_on_server) {
					char *cachename = mail_config_folder_to_cachename (folder, "cache-");

					cache = camel_uid_cache_new (cachename);
					if (cache) {
						GPtrArray *new_uids;
						
						new_uids = camel_uid_cache_get_new_uids (cache, uids);
						camel_folder_free_uids (folder, uids);
						uids = new_uids;
					}
					
					g_free (cachename);
				}
				
				filter_driver_filter_folder (filter, folder, FILTER_SOURCE_INCOMING,
							     uids, !input->keep_on_server, ex);
				
				if (cache) {
					/* save the cache for the next time we fetch mail! */
					camel_uid_cache_free_uids (uids);
					
					if (!camel_exception_is_set (ex))
						camel_uid_cache_save (cache);
					camel_uid_cache_destroy (cache);
				} else
					camel_folder_free_uids (folder, uids);
			} else {
				data->empty = TRUE;
			}
			
			/* sync and expunge */
			camel_folder_sync (folder, TRUE, ex);
			
			camel_object_unref (CAMEL_OBJECT (folder));
		} else {
			data->empty = TRUE;
		}
	}

	if (logfile)
		fclose (logfile);

	camel_folder_thaw (input->destination);

	mail_tool_camel_lock_down ();

	/*camel_object_unref (CAMEL_OBJECT (input->destination));*/
	gtk_object_unref (GTK_OBJECT (filter));
}

static void
cleanup_fetch_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	fetch_mail_input_t *input = (fetch_mail_input_t *) in_data;
	fetch_mail_data_t *data = (fetch_mail_data_t *) op_data;

	if (data->empty && !camel_exception_is_set (ex)) {
		GtkWidget *dialog;
		gchar *str;

		str = g_strdup_printf (_("There is no new mail at %s."),
				       input->source_url);
		dialog = gnome_ok_dialog (str);
		g_free (str);
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	}

	g_free (input->source_url);
	if (input->destination)
		camel_object_unref (CAMEL_OBJECT (input->destination));
}

static const mail_operation_spec op_fetch_mail = {
	describe_fetch_mail,
	sizeof (fetch_mail_data_t),
	setup_fetch_mail,
	do_fetch_mail,
	cleanup_fetch_mail
};

void
mail_do_fetch_mail (const gchar *source_url, gboolean keep_on_server,
		    CamelFolder *destination,
		    gpointer hook_func, gpointer hook_data)
{
	fetch_mail_input_t *input;
	
	g_return_if_fail (source_url != NULL);
	g_return_if_fail (destination == NULL ||
			  CAMEL_IS_FOLDER (destination));

	input = g_new (fetch_mail_input_t, 1);
	input->source_url = g_strdup (source_url);
	input->keep_on_server = keep_on_server;
	input->destination = destination;
	input->hook_func = hook_func;
	input->hook_data = hook_data;
	
	mail_operation_queue (&op_fetch_mail, input, TRUE);
}

/* ** FILTER ON DEMAND ********************************************************** */

/* why do we have this separate code, it is basically a copy of the code above,
   should be consolidated */

typedef struct filter_ondemand_input_s {
	CamelFolder *source;
	GPtrArray *uids;
} filter_ondemand_input_t;

static gchar *
describe_filter_ondemand (gpointer in_data, gboolean gerund)
{
	if (gerund)
		return g_strdup_printf (_("Filtering email on demand"));
	else
		return g_strdup_printf (_("Filter email on demand"));
}

static void
setup_filter_ondemand (gpointer in_data, gpointer op_data, CamelException *ex)
{
	filter_ondemand_input_t *input = (filter_ondemand_input_t *) in_data;
	
	camel_object_ref (CAMEL_OBJECT (input->source));
}

static void
do_filter_ondemand (gpointer in_data, gpointer op_data, CamelException *ex)
{
	filter_ondemand_input_t *input = (filter_ondemand_input_t *) in_data;
	FilterDriver *driver;
	FilterContext *context;
	FILE *logfile = NULL;
	int i;
	
	mail_tool_camel_lock_up ();
	if (camel_folder_get_message_count (input->source) == 0) {
		mail_tool_camel_lock_down ();
		return;
	}
	
	/* create the filter context */
	context = mail_load_evolution_rule_context ();
	
	if (((RuleContext *)context)->error) {
		gtk_object_unref (GTK_OBJECT (context));
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Cannot apply filters: failed to load filter rules.");
		
		mail_tool_camel_lock_down ();
		return;
	}
	
	/* setup filter driver - no default destination */
	driver = filter_driver_new (context, mail_tool_filter_get_folder_func, NULL);
	
	if (TRUE /* perform_logging */) {
		char *filename;
		
		filename = g_strdup_printf ("%s/evolution-filter-log", evolution_dir);
		logfile = fopen (filename, "a+");
		g_free (filename);
	}
	
	filter_driver_set_logfile (driver, logfile);
	filter_driver_set_status_func (driver, mail_op_report_status, NULL);
	
	for (i = 0; i < input->uids->len; i++) {
		CamelMimeMessage *message;
		CamelMessageInfo *info;
		
		message = camel_folder_get_message (input->source, input->uids->pdata[i], ex);
		info = (CamelMessageInfo *) camel_folder_get_message_info (input->source, input->uids->pdata[i]);
		
		/* filter the message - use "incoming" rules since we don't want special "demand" filters? */
		filter_driver_filter_message (driver, message, info, "", FILTER_SOURCE_INCOMING, ex);
	}
	
	if (logfile)
		fclose (logfile);
	
	/* sync the source folder */
	camel_folder_sync (input->source, FALSE, ex);
	
	gtk_object_unref (GTK_OBJECT (driver));
	mail_tool_camel_lock_down ();
}

static void
cleanup_filter_ondemand (gpointer in_data, gpointer op_data, CamelException *ex)
{
	filter_ondemand_input_t *input = (filter_ondemand_input_t *) in_data;
	int i;
	
	if (input->source)
		camel_object_unref (CAMEL_OBJECT (input->source));
	
	for (i = 0; i < input->uids->len; i++)
		g_free (input->uids->pdata[i]);
	g_ptr_array_free (input->uids, TRUE);
}

static const mail_operation_spec op_filter_ondemand = {
	describe_filter_ondemand,
	0,
	setup_filter_ondemand,
	do_filter_ondemand,
	cleanup_filter_ondemand
};

void
mail_do_filter_ondemand (CamelFolder *source, GPtrArray *uids)
{
	filter_ondemand_input_t *input;
	
	g_return_if_fail (source == NULL || CAMEL_IS_FOLDER (source));
	
	input = g_new (filter_ondemand_input_t, 1);
	input->source = source;
	input->uids = uids;
	
	mail_operation_queue (&op_filter_ondemand, input, TRUE);
}

/* ** SEND MAIL *********************************************************** */

typedef struct send_mail_input_s
{
	gchar *xport_uri;
	CamelMimeMessage *message;
	
	/* If done_folder != NULL, will add done_flags to
	 * the flags of the message done_uid in done_folder. */

	CamelFolder *done_folder;
	char *done_uid;
	guint32 done_flags;

	GtkWidget *composer;
}
send_mail_input_t;

static gchar *
describe_send_mail (gpointer in_data, gboolean gerund)
{
	send_mail_input_t *input = (send_mail_input_t *) in_data;

	if (gerund) {
		if (input->message->subject && input->message->subject[0])
			return g_strdup_printf (_("Sending \"%s\""),
						input->message->subject);
		else
			return
				g_strdup
				(_("Sending a message without a subject"));
	} else {
		if (input->message->subject && input->message->subject[0])
			return g_strdup_printf (_("Send \"%s\""),
						input->message->subject);
		else
			return g_strdup (_("Send a message without a subject"));
	}
}

static void
setup_send_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	send_mail_input_t *input = (send_mail_input_t *) in_data;
	
	camel_object_ref (CAMEL_OBJECT (input->message));
	if (input->done_folder)
		camel_object_ref (CAMEL_OBJECT (input->done_folder));
	if (input->composer) {
		gtk_object_ref (GTK_OBJECT (input->composer));
		gtk_widget_hide (GTK_WIDGET (input->composer));
	}
}

static void
do_send_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	send_mail_input_t *input = (send_mail_input_t *) in_data;
	extern CamelFolder *sent_folder;
	CamelMessageInfo *info;
	CamelTransport *xport;
	FilterContext *context;
	char *x_mailer;
	
	mail_tool_camel_lock_up ();
	x_mailer = g_strdup_printf ("Evolution %s (Developer Preview)", VERSION);
	camel_medium_add_header (CAMEL_MEDIUM (input->message), "X-Mailer",
				 x_mailer);
	g_free (x_mailer);
	camel_mime_message_set_date (input->message,
				     CAMEL_MESSAGE_DATE_CURRENT, 0);
	
	xport = camel_session_get_transport (session, input->xport_uri, ex);
	mail_tool_camel_lock_down ();
	if (camel_exception_is_set (ex))
		return;
	
	mail_tool_send_via_transport (xport, CAMEL_MEDIUM (input->message), ex);
	camel_object_unref (CAMEL_OBJECT (xport));
	
	if (camel_exception_is_set (ex))
		return;
	
	/* if we replied to a message, mark the appropriate flags and stuff */
	if (input->done_folder) {
		guint32 set;
		
		mail_tool_camel_lock_up ();
		set = camel_folder_get_message_flags (input->done_folder,
						      input->done_uid);
		camel_folder_set_message_flags (input->done_folder,
						input->done_uid,
						input->done_flags,
						input->done_flags);
		mail_tool_camel_lock_down ();
	}
	
	/* now lets run it through the outgoing filters */
	
	info = g_new0 (CamelMessageInfo, 1);
	info->flags = CAMEL_MESSAGE_SEEN;
	
	/* setup filter driver */
	context = mail_load_evolution_rule_context ();
	
	if (!((RuleContext *)context)->error) {
		FilterDriver *driver;
		FILE *logfile;
		
		driver = filter_driver_new (context, mail_tool_filter_get_folder_func, NULL);
		
		if (TRUE /* perform_logging */) {
			char *filename;
			
			filename = g_strdup_printf ("%s/evolution-filter-log", evolution_dir);
			logfile = fopen (filename, "a+");
			g_free (filename);
		}
		
		filter_driver_filter_message (driver, input->message, info, "", FILTER_SOURCE_OUTGOING, ex);
		
		gtk_object_unref (GTK_OBJECT (driver));
		
		if (logfile)
			fclose (logfile);
	}
	
	/* now to save the message in Sent */
	if (sent_folder) {
		mail_tool_camel_lock_up ();
		
		camel_folder_append_message (sent_folder, input->message, info, ex);
		g_free (info);
		
		mail_tool_camel_lock_down ();
	}
}

static void
cleanup_send_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	send_mail_input_t *input = (send_mail_input_t *) in_data;
	
	camel_object_unref (CAMEL_OBJECT (input->message));
	if (input->done_folder)
		camel_object_unref (CAMEL_OBJECT (input->done_folder));
	
	g_free (input->xport_uri);
	g_free (input->done_uid);
	
	if (input->composer) {
		if (!camel_exception_is_set (ex))
			gtk_widget_destroy (input->composer);
		else
			gtk_widget_show (input->composer);
	}
}

static const mail_operation_spec op_send_mail = {
	describe_send_mail,
	0,
	setup_send_mail,
	do_send_mail,
	cleanup_send_mail
};

void
mail_do_send_mail (const char *xport_uri,
		   CamelMimeMessage *message,
		   CamelFolder *done_folder,
		   const char *done_uid,
		   guint32 done_flags, GtkWidget *composer)
{
	send_mail_input_t *input;
	
	g_return_if_fail (xport_uri != NULL);
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	g_return_if_fail (done_folder == NULL ||
			  CAMEL_IS_FOLDER (done_folder));
	g_return_if_fail (done_folder == NULL || done_uid != NULL);
	
	input = g_new (send_mail_input_t, 1);
	input->xport_uri = g_strdup (xport_uri);
	input->message = message;
	input->done_folder = done_folder;
	input->done_uid = g_strdup (done_uid);
	input->done_flags = done_flags;
	input->composer = composer;
	
	mail_operation_queue (&op_send_mail, input, TRUE);
}

/* ** SEND MAIL QUEUE ***************************************************** */

typedef struct send_queue_input_s
{
	CamelFolder *folder_queue;
	gchar *xport_uri;
}
send_queue_input_t;

static gchar *
describe_send_queue (gpointer in_data, gboolean gerund)
{
	/*send_queue_input_t *input = (send_queue_input_t *) in_data;*/
	
	if (gerund) {
		return g_strdup_printf (_("Sending queue"));
	} else {
		return g_strdup_printf (_("Send queue"));
	}
}

static void
setup_send_queue (gpointer in_data, gpointer op_data, CamelException *ex)
{
	send_queue_input_t *input = (send_queue_input_t *) in_data;
	
	camel_object_ref (CAMEL_OBJECT (input->folder_queue));
}

static void
do_send_queue (gpointer in_data, gpointer op_data, CamelException *ex)
{
	send_queue_input_t *input = (send_queue_input_t *) in_data;
	extern CamelFolder *sent_folder;
	CamelTransport *xport;
	GPtrArray *uids;
	char *x_mailer;
	guint32 set;
	int i;
	
	uids = camel_folder_get_uids (input->folder_queue);
	if (!uids)
		return;
	
	x_mailer = g_strdup_printf ("Evolution %s (Developer Preview)",
				    VERSION);
	
	for (i = 0; i < uids->len; i++) {
		CamelMimeMessage *message;
		
		mail_tool_camel_lock_up ();
		
		message = camel_folder_get_message (input->folder_queue, uids->pdata[i], ex);
		if (camel_exception_is_set (ex))
			break;
		
		camel_medium_add_header (CAMEL_MEDIUM (message), "X-Mailer", x_mailer);
		
		camel_mime_message_set_date (message, CAMEL_MESSAGE_DATE_CURRENT, 0);
		
		xport = camel_session_get_transport (session, input->xport_uri, ex);
		mail_tool_camel_lock_down ();
		if (camel_exception_is_set (ex))
			break;
		
		mail_tool_send_via_transport (xport, CAMEL_MEDIUM (message), ex);
		camel_object_unref (CAMEL_OBJECT (xport));
		
		if (camel_exception_is_set (ex))
			break;
		
		mail_tool_camel_lock_up ();
		set = camel_folder_get_message_flags (input->folder_queue,
						      uids->pdata[i]);
		camel_folder_set_message_flags (input->folder_queue,
						uids->pdata[i],
						CAMEL_MESSAGE_DELETED, ~set);
		mail_tool_camel_lock_down ();
		
		/* now to save the message in Sent */
		if (sent_folder) {
			CamelMessageInfo *info;
			
			mail_tool_camel_lock_up ();
			
			info = g_new0 (CamelMessageInfo, 1);
			info->flags = CAMEL_MESSAGE_SEEN;
			camel_folder_append_message (sent_folder, message, info, ex);
			g_free (info);
			
			mail_tool_camel_lock_down ();
		}
	}
	
	g_free (x_mailer);
	
	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);
	g_ptr_array_free (uids, TRUE);
}

static void
cleanup_send_queue (gpointer in_data, gpointer op_data, CamelException *ex)
{
	send_queue_input_t *input = (send_queue_input_t *) in_data;
	
	camel_object_unref (CAMEL_OBJECT (input->folder_queue));
	
	g_free (input->xport_uri);
}

static const mail_operation_spec op_send_queue = {
	describe_send_queue,
	0,
	setup_send_queue,
	do_send_queue,
	cleanup_send_queue
};

void
mail_do_send_queue (CamelFolder *folder_queue,
		    const char *xport_uri)
{
	send_queue_input_t *input;

	g_return_if_fail (xport_uri != NULL);
	g_return_if_fail (CAMEL_IS_FOLDER (folder_queue));

	input = g_new (send_queue_input_t, 1);
	input->xport_uri = g_strdup (xport_uri);
	input->folder_queue = folder_queue;
	
	mail_operation_queue (&op_send_queue, input, TRUE);
}


/* ** APPEND MESSAGE TO FOLDER ******************************************** */

typedef struct append_mail_input_s
{
        CamelFolder *folder;
	CamelMimeMessage *message;
        CamelMessageInfo *info;
}
append_mail_input_t;

static gchar *
describe_append_mail (gpointer in_data, gboolean gerund)
{
	append_mail_input_t *input = (append_mail_input_t *) in_data;
	
	if (gerund) {
		if (input->message->subject && input->message->subject[0])
			return g_strdup_printf (_("Appending \"%s\""),
						input->message->subject);
		else
			return
				g_strdup (_("Appending a message without a subject"));
	} else {
		if (input->message->subject && input->message->subject[0])
			return g_strdup_printf (_("Appending \"%s\""),
						input->message->subject);
		else
			return g_strdup (_("Appending a message without a subject"));
	}
}

static void
setup_append_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	append_mail_input_t *input = (append_mail_input_t *) in_data;
	
	camel_object_ref (CAMEL_OBJECT (input->message));
	camel_object_ref (CAMEL_OBJECT (input->folder));
}

static void
do_append_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	append_mail_input_t *input = (append_mail_input_t *) in_data;
	
	camel_mime_message_set_date (input->message,
				     CAMEL_MESSAGE_DATE_CURRENT, 0);
	
        mail_tool_camel_lock_up ();
	
	/* now to save the message in the specified folder */
	camel_folder_append_message (input->folder, input->message, input->info, ex);
	
        mail_tool_camel_lock_down ();
}

static void
cleanup_append_mail (gpointer in_data, gpointer op_data, CamelException *ex)
{
	append_mail_input_t *input = (append_mail_input_t *) in_data;
	
	camel_object_unref (CAMEL_OBJECT (input->message));
        camel_object_unref (CAMEL_OBJECT (input->folder));
}

static const mail_operation_spec op_append_mail = {
	describe_append_mail,
	0,
	setup_append_mail,
	do_append_mail,
	cleanup_append_mail
};

void
mail_do_append_mail (CamelFolder *folder,
		     CamelMimeMessage *message,
		     CamelMessageInfo *info)
{
	append_mail_input_t *input;
	
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	input = g_new (append_mail_input_t, 1);
	input->folder = folder;
	input->message = message;
	input->info = info;
	
	mail_operation_queue (&op_append_mail, input, TRUE);
}

/* ** EXPUNGE FOLDER ****************************************************** */

static gchar *
describe_expunge_folder (gpointer in_data, gboolean gerund)
{
	CamelFolder *f = CAMEL_FOLDER (in_data);

	if (gerund)
		return g_strdup_printf (_("Expunging \"%s\""), mail_tool_get_folder_name (f));
	else
		return g_strdup_printf (_("Expunge \"%s\""), mail_tool_get_folder_name (f));
}

static void
setup_expunge_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	camel_object_ref (CAMEL_OBJECT (in_data));
}

static void
do_expunge_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	mail_tool_camel_lock_up ();
	camel_folder_expunge (CAMEL_FOLDER (in_data), ex);
	mail_tool_camel_lock_down ();
}

static void
cleanup_expunge_folder (gpointer in_data, gpointer op_data,
			CamelException *ex)
{
	camel_object_unref (CAMEL_OBJECT (in_data));
}

static const mail_operation_spec op_expunge_folder = {
	describe_expunge_folder,
	0,
	setup_expunge_folder,
	do_expunge_folder,
	cleanup_expunge_folder
};

void
mail_do_expunge_folder (CamelFolder *folder)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	mail_operation_queue (&op_expunge_folder, folder, FALSE);
}

/* ** TRANSFER MESSAGES **************************************************** */

typedef struct transfer_messages_input_s
{
	CamelFolder *source;
	GPtrArray *uids;
	gboolean delete_from_source;
	gchar *dest_uri;
}
transfer_messages_input_t;

static gchar *
describe_transfer_messages (gpointer in_data, gboolean gerund)
{
	transfer_messages_input_t *input = (transfer_messages_input_t *) in_data;
	char *format;
	
	if (gerund) {
		if (input->delete_from_source)
			format = _("Moving messages from \"%s\" into \"%s\"");
		else
			format = _("Copying messages from \"%s\" into \"%s\"");
	} else {
		if (input->delete_from_source)
			format = _("Move messages from \"%s\" into \"%s\"");
		else
			format = _("Copy messages from \"%s\" into \"%s\"");
	}

	return g_strdup_printf (format,
				mail_tool_get_folder_name (input->source), 
				input->dest_uri);
}

static void
setup_transfer_messages (gpointer in_data, gpointer op_data,
			 CamelException *ex)
{
	transfer_messages_input_t *input = (transfer_messages_input_t *) in_data;

	camel_object_ref (CAMEL_OBJECT (input->source));
}

static void
do_transfer_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	transfer_messages_input_t *input = (transfer_messages_input_t *) in_data;
	CamelFolder *dest;
	gint i;
	time_t last_update = 0;
	gchar *desc;
	void (*func) (CamelFolder *, const char *, 
		      CamelFolder *, 
		      CamelException *);

	if (input->delete_from_source) {
		func = camel_folder_move_message_to;
		desc = _("Moving");
	} else {
		func = camel_folder_copy_message_to;
		desc = _("Copying");
	}

	dest = mail_tool_uri_to_folder (input->dest_uri, ex);
	if (camel_exception_is_set (ex))
		return;

	mail_tool_camel_lock_up ();
	camel_folder_freeze (input->source);
	camel_folder_freeze (dest);

	for (i = 0; i < input->uids->len; i++) {
		const gboolean last_message = (i+1 == input->uids->len);
		time_t now;

		/*
		 * Update the time display ever 2 seconds
		 */
		time (&now);
		if (last_message || ((now - last_update) > 2)){
			mail_op_set_message (_("%s message %d of %d (uid \"%s\")"), desc,
					     i + 1, input->uids->len, (char *) input->uids->pdata[i]);
			last_update = now;
		}
		
		(func) (input->source,
			input->uids->pdata[i], dest,
			ex);
		g_free (input->uids->pdata[i]);
		if (camel_exception_is_set (ex))
			break;
	}

	camel_folder_thaw (input->source);
	camel_folder_thaw (dest);
	camel_object_unref (CAMEL_OBJECT (dest));
	mail_tool_camel_lock_down ();
}

static void
cleanup_transfer_messages (gpointer in_data, gpointer op_data,
			   CamelException *ex)
{
	transfer_messages_input_t *input = (transfer_messages_input_t *) in_data;

	camel_object_unref (CAMEL_OBJECT (input->source));
	g_free (input->dest_uri);
	g_ptr_array_free (input->uids, TRUE);
}

static const mail_operation_spec op_transfer_messages = {
	describe_transfer_messages,
	0,
	setup_transfer_messages,
	do_transfer_messages,
	cleanup_transfer_messages
};

void
mail_do_transfer_messages (CamelFolder *source, GPtrArray *uids,
			   gboolean delete_from_source,
			   gchar *dest_uri)
{
	transfer_messages_input_t *input;

	g_return_if_fail (CAMEL_IS_FOLDER (source));
	g_return_if_fail (uids != NULL);
	g_return_if_fail (dest_uri != NULL);

	input = g_new (transfer_messages_input_t, 1);
	input->source = source;
	input->uids = uids;
	input->delete_from_source = delete_from_source;
	input->dest_uri = g_strdup (dest_uri);

	mail_operation_queue (&op_transfer_messages, input, TRUE);
}

/* ** FLAG MESSAGES ******************************************************* */

typedef struct flag_messages_input_s
{
	CamelFolder *source;
	GPtrArray *uids;
	gboolean invert;
	guint32 mask;
	guint32 set;
	gboolean flag_all;
}
flag_messages_input_t;

static gchar *
describe_flag_messages (gpointer in_data, gboolean gerund)
{
	flag_messages_input_t *input = (flag_messages_input_t *) in_data;

	/* FIXME: change based on flags being applied? */

	if (gerund)
		return g_strdup_printf (_("Marking messages in folder \"%s\""),
					mail_tool_get_folder_name (input->source));
	else
		return g_strdup_printf (_("Mark messages in folder \"%s\""),
					mail_tool_get_folder_name (input->source));
}

static void
setup_flag_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	flag_messages_input_t *input = (flag_messages_input_t *) in_data;

	camel_object_ref (CAMEL_OBJECT (input->source));
}

static void
do_flag_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	flag_messages_input_t *input = (flag_messages_input_t *) in_data;
	gint i;
	time_t last_update = 0;

	mail_tool_camel_lock_up ();
	camel_folder_freeze (input->source);
	if (input->uids == NULL) 
		input->uids = camel_folder_get_uids (input->source);
	mail_tool_camel_lock_down ();

	for (i = 0; i < input->uids->len; i++) {
		const gboolean last_message = (i+1 == input->uids->len);
		time_t now;

		time (&now);
		if (last_message || ((now - last_update) > 2)){
			mail_op_set_message (_("Marking message %d of %d"), i + 1,
					     input->uids->len);
			last_update = now;
		}

		if (input->invert) {
			const CamelMessageInfo *info;

			mail_tool_camel_lock_up ();
			info = camel_folder_get_message_info (input->source, input->uids->pdata[i]);
			camel_folder_set_message_flags (input->source, input->uids->pdata[i],
							input->mask, ~info->flags);
			mail_tool_camel_lock_down ();
		} else {
			mail_tool_set_uid_flags (input->source, input->uids->pdata[i],
						 input->mask, input->set);
		}

		if (input->flag_all == FALSE)
			g_free (input->uids->pdata[i]);
	}

	mail_tool_camel_lock_up ();
	if (input->flag_all) {
		camel_folder_free_uids (input->source, input->uids);
		input->uids = NULL;
	}
	camel_folder_thaw (input->source);
	mail_tool_camel_lock_down ();
}

static void
cleanup_flag_messages (gpointer in_data, gpointer op_data,
		       CamelException *ex)
{
	flag_messages_input_t *input = (flag_messages_input_t *) in_data;

	camel_object_unref (CAMEL_OBJECT (input->source));

	if (input->uids)
		g_ptr_array_free (input->uids, TRUE);
}

static const mail_operation_spec op_flag_messages = {
	describe_flag_messages,
	0,
	setup_flag_messages,
	do_flag_messages,
	cleanup_flag_messages
};

void
mail_do_flag_messages (CamelFolder *source, GPtrArray *uids,
		       gboolean invert,
		       guint32 mask, guint32 set)
{
	flag_messages_input_t *input;

	g_return_if_fail (CAMEL_IS_FOLDER (source));
	g_return_if_fail (uids != NULL);

	input = g_new (flag_messages_input_t, 1);
	input->source = source;
	input->uids = uids;
	input->invert = invert;
	input->mask = mask;
	input->set = set;
	input->flag_all = FALSE;

	mail_operation_queue (&op_flag_messages, input, TRUE);
}

void
mail_do_flag_all_messages (CamelFolder *source, gboolean invert,
			   guint32 mask, guint32 set)
{
	flag_messages_input_t *input;

	g_return_if_fail (CAMEL_IS_FOLDER (source));

	input = g_new (flag_messages_input_t, 1);
	input->source = source;
	input->uids = NULL;
	input->invert = invert;
	input->mask = mask;
	input->set = set;
	input->flag_all = TRUE;

	mail_operation_queue (&op_flag_messages, input, TRUE);
}

/* ** SCAN SUBFOLDERS ***************************************************** */

typedef struct scan_subfolders_input_s
{
	CamelStore *store;
	EvolutionStorage *storage;
}
scan_subfolders_input_t;

typedef struct scan_subfolders_folderinfo_s
{
	char *path;
	char *name;
	char *uri;
	gboolean highlighted;
}
scan_subfolders_folderinfo_t;

typedef struct scan_subfolders_op_s
{
	GPtrArray *new_folders;
}
scan_subfolders_op_t;

static gchar *
describe_scan_subfolders (gpointer in_data, gboolean gerund)
{
	scan_subfolders_input_t *input = (scan_subfolders_input_t *) in_data;
	char *name;

	name = camel_service_get_name (CAMEL_SERVICE (input->store), TRUE);
	if (gerund)
		return g_strdup_printf (_("Scanning folders in \"%s\""), name);
	else
		return g_strdup_printf (_("Scan folders in \"%s\""), name);
	g_free (name);
}

static void
setup_scan_subfolders (gpointer in_data, gpointer op_data,
		       CamelException *ex)
{
	scan_subfolders_input_t *input = (scan_subfolders_input_t *) in_data;
	scan_subfolders_op_t *data = (scan_subfolders_op_t *) op_data;
	
	camel_object_ref (CAMEL_OBJECT (input->store));
	gtk_object_ref (GTK_OBJECT (input->storage));

	data->new_folders = g_ptr_array_new ();
}

static void
add_folders (GPtrArray *folders, const char *prefix, CamelFolderInfo *fi)
{
	scan_subfolders_folderinfo_t *info;

	info = g_new (scan_subfolders_folderinfo_t, 1);
	info->path = g_strdup_printf ("%s/%s", prefix, fi->name);
	if (fi->unread_message_count > 0) {
		info->name = g_strdup_printf ("%s (%d)", fi->name,
					      fi->unread_message_count);
		info->highlighted = TRUE;
	} else {
		info->name = g_strdup (fi->name);
		info->highlighted = FALSE;
	}
	info->uri = g_strdup (fi->url);
	g_ptr_array_add (folders, info);
	if (fi->child)
		add_folders (folders, info->path, fi->child);
	if (fi->sibling)
		add_folders (folders, prefix, fi->sibling);
}

static void
do_scan_subfolders (gpointer in_data, gpointer op_data, CamelException *ex)
{
	scan_subfolders_input_t *input = (scan_subfolders_input_t *) in_data;
	scan_subfolders_op_t *data = (scan_subfolders_op_t *) op_data;
	CamelFolderInfo *tree;

	tree = camel_store_get_folder_info (input->store, NULL, FALSE,
					    TRUE, TRUE, ex);
	if (tree) {
		add_folders (data->new_folders, "", tree);
		camel_store_free_folder_info (input->store, tree);
	}
}

static void
cleanup_scan_subfolders (gpointer in_data, gpointer op_data,
			 CamelException *ex)
{
	scan_subfolders_input_t *input = (scan_subfolders_input_t *) in_data;
	scan_subfolders_op_t *data = (scan_subfolders_op_t *) op_data;
	scan_subfolders_folderinfo_t *info;
	int i;

	for (i = 0; i < data->new_folders->len; i++) {
		info = data->new_folders->pdata[i];
		evolution_storage_new_folder (input->storage, info->path,
					      info->name, "mail",
					      info->uri ? info->uri : "",
					      _("(No description)"),
					      info->highlighted);

		g_free (info->uri);
		g_free (info->name);
		g_free (info->path);
		g_free (info);
	}
	g_ptr_array_free (data->new_folders, TRUE);

	gtk_object_unref (GTK_OBJECT (input->storage));
	camel_object_unref (CAMEL_OBJECT (input->store));
}

static const mail_operation_spec op_scan_subfolders = {
	describe_scan_subfolders,
	sizeof (scan_subfolders_op_t),
	setup_scan_subfolders,
	do_scan_subfolders,
	cleanup_scan_subfolders
};

void
mail_do_scan_subfolders (CamelStore *store, EvolutionStorage *storage)
{
	scan_subfolders_input_t *input;
	
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (EVOLUTION_IS_STORAGE (storage));
	
	input = g_new (scan_subfolders_input_t, 1);
	input->store = store;
	input->storage = storage;
	
	mail_operation_queue (&op_scan_subfolders, input, TRUE);
}

/* ** ATTACH MESSAGE ****************************************************** */

typedef struct attach_message_input_s
{
	EMsgComposer *composer;
	CamelFolder *folder;
	gchar *uid;
}
attach_message_input_t;

typedef struct attach_message_data_s
{
	CamelMimePart *part;
}
attach_message_data_t;

static gchar *
describe_attach_message (gpointer in_data, gboolean gerund)
{
	attach_message_input_t *input = (attach_message_input_t *) in_data;

	if (gerund)
		return
			g_strdup_printf
			(_("Attaching messages from folder \"%s\""),
			 mail_tool_get_folder_name (input->folder));
	else
		return g_strdup_printf (_("Attach messages from \"%s\""),
					mail_tool_get_folder_name (input->folder));
}

static void
setup_attach_message (gpointer in_data, gpointer op_data, CamelException *ex)
{
	attach_message_input_t *input = (attach_message_input_t *) in_data;

	camel_object_ref (CAMEL_OBJECT (input->folder));
	gtk_object_ref (GTK_OBJECT (input->composer));
}

static void
do_attach_message (gpointer in_data, gpointer op_data, CamelException *ex)
{
	attach_message_input_t *input = (attach_message_input_t *) in_data;
	attach_message_data_t *data = (attach_message_data_t *) op_data;

	CamelMimeMessage *message;
	CamelMimePart *part;

	mail_tool_camel_lock_up ();
	message = camel_folder_get_message (input->folder, input->uid, ex);
	if (!message) {
		mail_tool_camel_lock_down ();
		return;
	}

	part = mail_tool_make_message_attachment (message);
	camel_object_unref (CAMEL_OBJECT (message));
	mail_tool_camel_lock_down ();
	if (!part)
		return;

	data->part = part;
}

static void
cleanup_attach_message (gpointer in_data, gpointer op_data,
			CamelException *ex)
{
	attach_message_input_t *input = (attach_message_input_t *) in_data;
	attach_message_data_t *data = (attach_message_data_t *) op_data;

	e_msg_composer_attach (input->composer, data->part);
	camel_object_unref (CAMEL_OBJECT (data->part));
	camel_object_unref (CAMEL_OBJECT (input->folder));
	gtk_object_unref (GTK_OBJECT (input->composer));
	g_free (input->uid);
}

static const mail_operation_spec op_attach_message = {
	describe_attach_message,
	sizeof (attach_message_data_t),
	setup_attach_message,
	do_attach_message,
	cleanup_attach_message
};

void
mail_do_attach_message (CamelFolder *folder, const char *uid,
			EMsgComposer *composer)
{
	attach_message_input_t *input;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uid != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	input = g_new (attach_message_input_t, 1);
	input->folder = folder;
	input->uid = g_strdup (uid);
	input->composer = composer;

	mail_operation_queue (&op_attach_message, input, TRUE);
}

/* ** FORWARD MESSAGES **************************************************** */

typedef struct forward_messages_input_s
{
	CamelMimeMessage *basis;
	CamelFolder *source;
	GPtrArray *uids;
	EMsgComposer *composer;
}
forward_messages_input_t;

typedef struct forward_messages_data_s
{
	gchar *subject;
	GPtrArray *parts;
}
forward_messages_data_t;

static gchar *
describe_forward_messages (gpointer in_data, gboolean gerund)
{
	forward_messages_input_t *input = (forward_messages_input_t *) in_data;

	if (gerund) {
		if (input->basis->subject)
			return g_strdup_printf (_("Forwarding messages \"%s\""),
						input->basis->subject);
		else
			return
				g_strdup_printf
				(_("Forwarding a message without a subject"));
	} else {
		if (input->basis->subject)
			return g_strdup_printf (_("Forward message \"%s\""),
						input->basis->subject);
		else
			return
				g_strdup_printf
				(_("Forward a message without a subject"));
	}
}

static void
setup_forward_messages (gpointer in_data, gpointer op_data,
			CamelException *ex)
{
	forward_messages_input_t *input = (forward_messages_input_t *) in_data;

	camel_object_ref (CAMEL_OBJECT (input->basis));
	camel_object_ref (CAMEL_OBJECT (input->source));
	gtk_object_ref (GTK_OBJECT (input->composer));
}

static void
do_forward_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	forward_messages_input_t *input = (forward_messages_input_t *) in_data;
	forward_messages_data_t *data = (forward_messages_data_t *) op_data;
	time_t last_update = 0;
	CamelMimeMessage *message;
	CamelMimePart *part;
	int i;

	data->parts = g_ptr_array_new ();

	mail_tool_camel_lock_up ();
	for (i = 0; i < input->uids->len; i++) {
		const int last_message = (i+1 == input->uids->len);
		time_t now;

		/*
		 * Update the time display ever 2 seconds
		 */
		time (&now);
		if (last_message || ((now - last_update) > 2)){
			mail_op_set_message (_("Retrieving message number %d of %d (uid \"%s\")"),
					     i + 1, input->uids->len, (char *) input->uids->pdata[i]);
			last_update = now;
		}

		
		message =
			camel_folder_get_message (input->source,
						  input->uids->pdata[i], ex);
		g_free (input->uids->pdata[i]);
		if (!message) {
			mail_tool_camel_lock_down ();
			return;
		}
		part = mail_tool_make_message_attachment (message);
		if (!part) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Failed to generate mime part from "
					       "message while generating forwarded message."));
			mail_tool_camel_lock_down ();
			return;
		}
		camel_object_unref (CAMEL_OBJECT (message));
		g_ptr_array_add (data->parts, part);
	}

	mail_tool_camel_lock_down ();

	data->subject = mail_tool_generate_forward_subject (input->basis);
}

static void
cleanup_forward_messages (gpointer in_data, gpointer op_data,
			  CamelException *ex)
{
	forward_messages_input_t *input =

		(forward_messages_input_t *) in_data;
	forward_messages_data_t *data = (forward_messages_data_t *) op_data;

	int i;

	for (i = 0; i < data->parts->len; i++) {
		e_msg_composer_attach (input->composer,
				       data->parts->pdata[i]);
		camel_object_unref (CAMEL_OBJECT (data->parts->pdata[i]));
	}
	camel_object_unref (CAMEL_OBJECT (input->source));

	e_msg_composer_set_headers (input->composer, NULL, NULL, NULL,
				    data->subject);

	gtk_object_unref (GTK_OBJECT (input->composer));
	g_free (data->subject);
	g_ptr_array_free (data->parts, TRUE);
	g_ptr_array_free (input->uids, TRUE);
	gtk_widget_show (GTK_WIDGET (input->composer));
}

static const mail_operation_spec op_forward_messages = {
	describe_forward_messages,
	sizeof (forward_messages_data_t),
	setup_forward_messages,
	do_forward_messages,
	cleanup_forward_messages
};

void
mail_do_forward_message (CamelMimeMessage *basis,
			 CamelFolder *source,
			 GPtrArray *uids, EMsgComposer *composer)
{
	forward_messages_input_t *input;

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (basis));
	g_return_if_fail (CAMEL_IS_FOLDER (source));
	g_return_if_fail (uids != NULL);
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	input = g_new (forward_messages_input_t, 1);
	input->basis = basis;
	input->source = source;
	input->uids = uids;
	input->composer = composer;

	mail_operation_queue (&op_forward_messages, input, TRUE);
}

/* ** LOAD FOLDER ********************************************************* */

typedef struct load_folder_input_s
{
	FolderBrowser *fb;
	gchar *url;
}
load_folder_input_t;

static gchar *
describe_load_folder (gpointer in_data, gboolean gerund)
{
	load_folder_input_t *input = (load_folder_input_t *) in_data;

	if (gerund) {
		return g_strdup_printf (_("Loading \"%s\""), input->url);
	} else {
		return g_strdup_printf (_("Load \"%s\""), input->url);
	}
}

static void
setup_load_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	load_folder_input_t *input = (load_folder_input_t *) in_data;

	gtk_object_ref (GTK_OBJECT (input->fb));

	if (input->fb->uri)
		g_free (input->fb->uri);

	input->fb->uri = input->url;
}

static void
do_load_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	load_folder_input_t *input = (load_folder_input_t *) in_data;

	CamelFolder *folder;

	folder = mail_tool_uri_to_folder (input->url, ex);
	if (!folder)
		return;

	if (input->fb->folder) {
		mail_tool_camel_lock_up ();
		camel_object_unref (CAMEL_OBJECT (input->fb->folder));
		mail_tool_camel_lock_down ();
	}

	input->fb->folder = folder;
}

static void
cleanup_load_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	load_folder_input_t *input = (load_folder_input_t *) in_data;

	if (input->fb->folder) {
		gtk_widget_set_sensitive (GTK_WIDGET (input->fb->search->entry),
					  camel_folder_has_search_capability (input->
									      fb->
									      folder));
		gtk_widget_set_sensitive (GTK_WIDGET (input->fb->search->option),
					  camel_folder_has_search_capability (input->
									      fb->
									      folder));
		message_list_set_folder (input->fb->message_list, input->fb->folder);
	}

	/*g_free (input->url); = fb->uri now */
}

static const mail_operation_spec op_load_folder = {
	describe_load_folder,
	0,
	setup_load_folder,
	do_load_folder,
	cleanup_load_folder
};

void
mail_do_load_folder (FolderBrowser *fb, const char *url)
{
	load_folder_input_t *input;

	g_return_if_fail (IS_FOLDER_BROWSER (fb));
	g_return_if_fail (url != NULL);

	input = g_new (load_folder_input_t, 1);
	input->fb = fb;
	input->url = g_strdup (url);

	mail_operation_queue (&op_load_folder, input, TRUE);
}

/* ** CREATE FOLDER ******************************************************* */

typedef struct create_folder_input_s
{
	GNOME_Evolution_ShellComponentListener listener;
	char *uri;
	char *type;
}
create_folder_input_t;

typedef struct create_folder_data_s
{
	GNOME_Evolution_ShellComponentListener_Result result;
}
create_folder_data_t;

static gchar *
describe_create_folder (gpointer in_data, gboolean gerund)
{
	create_folder_input_t *input = (create_folder_input_t *) in_data;

	if (gerund) {
		return g_strdup_printf (_("Creating \"%s\""), input->uri);
	} else {
		return g_strdup_printf (_("Create \"%s\""), input->uri);
	}
}

static void
do_create_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	create_folder_input_t *input = (create_folder_input_t *) in_data;
	create_folder_data_t *data = (create_folder_data_t *) op_data;

	CamelFolder *folder;
	gchar *camel_url;

	if (strcmp (input->type, "mail") != 0)
		data->result =
			GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE;
	else {
		camel_url = g_strdup_printf ("mbox://%s", input->uri);
		/* FIXME: supply a way to make indexes optional */
		folder = mail_tool_get_folder_from_urlname (camel_url,
							    "mbox", CAMEL_STORE_FOLDER_CREATE
							    |CAMEL_STORE_FOLDER_BODY_INDEX, ex);
		g_free (camel_url);

		if (!camel_exception_is_set (ex)) {
			camel_object_unref (CAMEL_OBJECT (folder));
			data->result = GNOME_Evolution_ShellComponentListener_OK;
		} else {
			data->result =
				GNOME_Evolution_ShellComponentListener_INVALID_URI;
		}
	}
}

static void
cleanup_create_folder (gpointer in_data, gpointer op_data,
		       CamelException *ex)
{
	create_folder_input_t *input = (create_folder_input_t *) in_data;
	create_folder_data_t *data = (create_folder_data_t *) op_data;

	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellComponentListener_notifyResult (input->listener,
							data->result, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Exception while reporting result to shell "
				       "component listener."));
	CORBA_Object_release (input->listener, &ev);

	g_free (input->uri);
	g_free (input->type);

	CORBA_exception_free (&ev);
}

static const mail_operation_spec op_create_folder = {
	describe_create_folder,
	sizeof (create_folder_data_t),
	NULL,
	do_create_folder,
	cleanup_create_folder
};

void
mail_do_create_folder (const GNOME_Evolution_ShellComponentListener listener,
		       const char *uri, const char *type)
{
	CORBA_Environment ev;
	create_folder_input_t *input;

	g_return_if_fail (uri != NULL);
	g_return_if_fail (type != NULL);

	input = g_new (create_folder_input_t, 1);
	CORBA_exception_init (&ev);
	input->listener = CORBA_Object_duplicate (listener, &ev);
	CORBA_exception_free (&ev);
	input->uri = g_strdup (uri);
	input->type = g_strdup (type);

	mail_operation_queue (&op_create_folder, input, FALSE);
}

/* ** SYNC FOLDER ********************************************************* */

static gchar *
describe_sync_folder (gpointer in_data, gboolean gerund)
{
	CamelFolder *f = CAMEL_FOLDER (in_data);

	if (gerund) {
		return g_strdup_printf (_("Synchronizing \"%s\""), mail_tool_get_folder_name (f));
	} else {
		return g_strdup_printf (_("Synchronize \"%s\""), mail_tool_get_folder_name (f));
	}
}

static void
setup_sync_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	camel_object_ref (CAMEL_OBJECT (in_data));
}

static void
do_sync_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	mail_tool_camel_lock_up ();
	camel_folder_sync (CAMEL_FOLDER (in_data), FALSE, ex);
	mail_tool_camel_lock_down ();
}

static void
cleanup_sync_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	camel_object_unref (CAMEL_OBJECT (in_data));
}

static const mail_operation_spec op_sync_folder = {
	describe_sync_folder,
	0,
	setup_sync_folder,
	do_sync_folder,
	cleanup_sync_folder
};

void
mail_do_sync_folder (CamelFolder *folder)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	mail_operation_queue (&op_sync_folder, folder, FALSE);
}

/* ** DISPLAY MESSAGE ***************************************************** */

typedef struct display_message_input_s
{
	MessageList *ml;
	MailDisplay *md;
	gchar *uid;
	gint (*timeout) (gpointer);
}
display_message_input_t;

typedef struct display_message_data_s
{
	CamelMimeMessage *msg;
}
display_message_data_t;

static gchar *
describe_display_message (gpointer in_data, gboolean gerund)
{
	display_message_input_t *input = (display_message_input_t *) in_data;

	if (gerund) {
		if (input->uid)
			return g_strdup_printf (_("Displaying message UID \"%s\""),
						input->uid);
		else
			return g_strdup (_("Clearing message display"));
	} else {
		if (input->uid)
			return g_strdup_printf (_("Display message UID \"%s\""),
						input->uid);
		else
			return g_strdup (_("Clear message display"));
	}
}

static void
setup_display_message (gpointer in_data, gpointer op_data,
		       CamelException *ex)
{
	display_message_input_t *input = (display_message_input_t *) in_data;
	display_message_data_t *data = (display_message_data_t *) op_data;

	data->msg = NULL;
	gtk_object_ref (GTK_OBJECT (input->ml));
}

static void
do_display_message (gpointer in_data, gpointer op_data, CamelException *ex)
{
	display_message_input_t *input = (display_message_input_t *) in_data;
	display_message_data_t *data = (display_message_data_t *) op_data;

	if (input->uid == NULL) {
		data->msg = NULL;
		return;
	}

	data->msg = camel_folder_get_message (input->ml->folder, input->uid, ex);
}

static void
cleanup_display_message (gpointer in_data, gpointer op_data,
			 CamelException *ex)
{
	display_message_input_t *input = (display_message_input_t *) in_data;
	display_message_data_t *data = (display_message_data_t *) op_data;
	MailDisplay *md = input->md;

	if (data->msg == NULL) {
		mail_display_set_message (md, NULL);
	} else {
		gint timeout = mail_config_mark_as_seen_timeout ();

		if (input->ml->seen_id)
			gtk_timeout_remove (input->ml->seen_id);

		mail_display_set_message (md, CAMEL_MEDIUM (data->msg));
		camel_object_unref (CAMEL_OBJECT (data->msg));

		if (timeout > 0) {
			input->ml->seen_id = gtk_timeout_add (timeout, 
							      input->timeout, 
							      input->ml);
		} else {
			input->ml->seen_id = 0;
			input->timeout (input->ml);
		}
	}

	if (input->uid)
		g_free (input->uid);
	gtk_object_unref (GTK_OBJECT (input->ml));
}

static const mail_operation_spec op_display_message = {
	describe_display_message,
	sizeof (display_message_data_t),
	setup_display_message,
	do_display_message,
	cleanup_display_message
};

void
mail_do_display_message (MessageList *ml, MailDisplay *md, const char *uid,
			 gint (*timeout) (gpointer))
{
	display_message_input_t *input;

	g_return_if_fail (IS_MESSAGE_LIST (ml));
	g_return_if_fail (timeout != NULL);

	if (uid == NULL) {
		mail_display_set_message (md, NULL);
		return;
	}

	input = g_new (display_message_input_t, 1);
	input->ml = ml;
	input->md = md;
	input->uid = g_strdup (uid);
	input->timeout = timeout;

	mail_operation_queue (&op_display_message, input, TRUE);
}

/* ** EDIT MESSAGES ******************************************************* */

typedef struct edit_messages_input_s {
	CamelFolder *folder;
	GPtrArray *uids;
	GtkSignalFunc signal;
} edit_messages_input_t;

typedef struct edit_messages_data_s {
	GPtrArray *messages;
} edit_messages_data_t;

static gchar *
describe_edit_messages (gpointer in_data, gboolean gerund)
{
	edit_messages_input_t *input = (edit_messages_input_t *) in_data;

	if (gerund)
		return g_strdup_printf
			(_("Opening messages from folder \"%s\""),
			 mail_tool_get_folder_name (input->folder));
	else
		return g_strdup_printf (_("Open messages from \"%s\""),
					mail_tool_get_folder_name (input->folder));
}

static void
setup_edit_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	edit_messages_input_t *input = (edit_messages_input_t *) in_data;
	
	camel_object_ref (CAMEL_OBJECT (input->folder));
}

static void
do_edit_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	edit_messages_input_t *input = (edit_messages_input_t *) in_data;
	edit_messages_data_t *data = (edit_messages_data_t *) op_data;
	
	int i;
	
	data->messages = g_ptr_array_new ();
	
	for (i = 0; i < input->uids->len; i++) {
		CamelMimeMessage *message;
		
		mail_tool_camel_lock_up ();
		message = camel_folder_get_message (input->folder, input->uids->pdata[i], ex);
		mail_tool_camel_lock_down ();
		
		if (message)
			g_ptr_array_add (data->messages, message);
		
		g_free (input->uids->pdata[i]);
	}
}

static void
cleanup_edit_messages (gpointer in_data, gpointer op_data,
			CamelException *ex)
{
	edit_messages_input_t *input = (edit_messages_input_t *) in_data;
	edit_messages_data_t *data = (edit_messages_data_t *) op_data;

	int i;

	for (i = 0; i < data->messages->len; i++) {
		EMsgComposer *composer;

		composer = e_msg_composer_new_with_message (data->messages->pdata[i]);
		camel_object_unref (CAMEL_OBJECT (data->messages->pdata[i]));
		if (!composer)
			continue;

		if (input->signal)
			gtk_signal_connect (GTK_OBJECT (composer), "send", 
					    input->signal, NULL);

		gtk_widget_show (GTK_WIDGET (composer));
	}

	g_ptr_array_free (input->uids, TRUE);
	g_ptr_array_free (data->messages, TRUE);
	camel_object_unref (CAMEL_OBJECT (input->folder));

}

static const mail_operation_spec op_edit_messages = {
	describe_edit_messages,
	sizeof (edit_messages_data_t),
	setup_edit_messages,
	do_edit_messages,
	cleanup_edit_messages
};

void
mail_do_edit_messages (CamelFolder *folder, GPtrArray *uids,
		       GtkSignalFunc signal)
{
	edit_messages_input_t *input;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	
	input = g_new (edit_messages_input_t, 1);
	input->folder = folder;
	input->uids = uids;
	input->signal = signal;

	mail_operation_queue (&op_edit_messages, input, TRUE);
}

/* ** SETUP FOLDER ****************************************************** */

typedef struct setup_folder_input_s {
	gchar *name;
	CamelFolder **folder;
} setup_folder_input_t;

static gchar *
describe_setup_folder (gpointer in_data, gboolean gerund)
{
	setup_folder_input_t *input = (setup_folder_input_t *) in_data;

	if (gerund)
		return g_strdup_printf (_("Loading %s Folder"), input->name);
	else
		return g_strdup_printf (_("Load %s Folder"), input->name);
}

static void
do_setup_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	setup_folder_input_t *input = (setup_folder_input_t *) in_data;
	gchar *url;

	url = g_strdup_printf ("mbox://%s/local/%s", evolution_dir,
			       input->name);
	*(input->folder) = mail_tool_get_folder_from_urlname (url, "mbox",
							      CAMEL_STORE_FOLDER_CREATE
							      |CAMEL_STORE_FOLDER_BODY_INDEX,
							      ex);
	g_free (url);
}

static void
cleanup_setup_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	setup_folder_input_t *input = (setup_folder_input_t *) in_data;

	g_free (input->name);
}

static const mail_operation_spec op_setup_folder = {
	describe_setup_folder,
	0,
	NULL,
	do_setup_folder,
	cleanup_setup_folder
};

void
mail_do_setup_folder (const char *name, CamelFolder **folder)
{
	setup_folder_input_t *input;

	g_return_if_fail (name != NULL);
	g_return_if_fail (folder != NULL);

	input = g_new (setup_folder_input_t, 1);
	input->name = g_strdup (name);
	input->folder = folder;
	mail_operation_queue (&op_setup_folder, input, TRUE);
}

/* ** VIEW MESSAGES ******************************************************* */

typedef struct view_messages_input_s {
	CamelFolder *folder;
	GPtrArray *uids;
	FolderBrowser *fb;
} view_messages_input_t;

typedef struct view_messages_data_s {
	GPtrArray *messages;
} view_messages_data_t;

static gchar *
describe_view_messages (gpointer in_data, gboolean gerund)
{
	view_messages_input_t *input = (view_messages_input_t *) in_data;

	if (gerund)
		return g_strdup_printf
			(_("Viewing messages from folder \"%s\""),
			 mail_tool_get_folder_name (input->folder));
	else
		return g_strdup_printf (_("View messages from \"%s\""),
					mail_tool_get_folder_name (input->folder));
}

static void
setup_view_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	view_messages_input_t *input = (view_messages_input_t *) in_data;

	camel_object_ref (CAMEL_OBJECT (input->folder));
	gtk_object_ref (GTK_OBJECT (input->fb));
}

static void
do_view_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	view_messages_input_t *input = (view_messages_input_t *) in_data;
	view_messages_data_t *data = (view_messages_data_t *) op_data;

	int i;

	data->messages = g_ptr_array_new ();

	for (i = 0; i < input->uids->len; i++) {
		CamelMimeMessage *message;

		mail_op_set_message (_("Retrieving message %d of %d (uid \"%s\")"),
				     i + 1, input->uids->len, (char *)input->uids->pdata[i]);

		mail_tool_camel_lock_up ();
		message = camel_folder_get_message (input->folder, input->uids->pdata[i], ex);
		mail_tool_camel_lock_down ();

		g_ptr_array_add (data->messages, message);
	}
}

static void
cleanup_view_messages (gpointer in_data, gpointer op_data,
		       CamelException *ex)
{
	view_messages_input_t *input = (view_messages_input_t *) in_data;
	view_messages_data_t *data = (view_messages_data_t *) op_data;

	int i;

	for (i = 0; i < data->messages->len; i++) {
		CamelMimeMessage *msg;
		gchar *uid;
		GtkWidget *view;

		if (data->messages->pdata[i] == NULL)
			continue;

		msg = data->messages->pdata[i];
		uid = input->uids->pdata[i];

		view = mail_view_create (input->folder, uid, msg);
		gtk_widget_show (view);

		/*Owned by the mail_display now*/
		camel_object_unref (CAMEL_OBJECT (data->messages->pdata[i]));
		g_free (uid);
	}

	g_ptr_array_free (input->uids, TRUE);
	g_ptr_array_free (data->messages, TRUE);
	camel_object_unref (CAMEL_OBJECT (input->folder));
	gtk_object_unref (GTK_OBJECT (input->fb));
}

static const mail_operation_spec op_view_messages = {
	describe_view_messages,
	sizeof (view_messages_data_t),
	setup_view_messages,
	do_view_messages,
	cleanup_view_messages
};

void
mail_do_view_messages (CamelFolder *folder, GPtrArray *uids,
		       FolderBrowser *fb)
{
	view_messages_input_t *input;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);
	g_return_if_fail (IS_FOLDER_BROWSER (fb));

	input = g_new (view_messages_input_t, 1);
	input->folder = folder;
	input->uids = uids;
	input->fb = fb;

	mail_operation_queue (&op_view_messages, input, TRUE);
}
