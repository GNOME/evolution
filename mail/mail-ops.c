/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-ops.c: callbacks for the mail toolbar/menus */

/* 
 * Author : 
 *  Dan Winship <danw@helixcode.com>
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
#include "mail.h"
#include "mail-threads.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "composer/e-msg-composer.h"

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

static gchar *describe_fetch_mail (gpointer in_data, gboolean gerund);
static void setup_fetch_mail (gpointer in_data, gpointer op_data,
			      CamelException * ex);
static void do_fetch_mail (gpointer in_data, gpointer op_data,
			   CamelException * ex);
static void cleanup_fetch_mail (gpointer in_data, gpointer op_data,
				CamelException * ex);

static gchar *
describe_fetch_mail (gpointer in_data, gboolean gerund)
{
	fetch_mail_input_t *input = (fetch_mail_input_t *) in_data;
	CamelStore *source;
	char *name;

	source = camel_session_get_store (session, input->source_url, NULL);
	if (source) {
		name = camel_service_get_name (CAMEL_SERVICE (source), FALSE);
		camel_object_unref (CAMEL_OBJECT (source));
	} else
		name = input->source_url;

	if (gerund)
		return g_strdup_printf ("Fetching email from %s", name);
	else
		return g_strdup_printf ("Fetch email from %s", name);
}

static void
setup_fetch_mail (gpointer in_data, gpointer op_data, CamelException * ex)
{
	fetch_mail_input_t *input = (fetch_mail_input_t *) in_data;
	fetch_mail_data_t *data = (fetch_mail_data_t *) op_data;

	if (!input->source_url) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "You have no remote mail source configured "
				     "to fetch mail from.");
		return;
	}

	if (input->destination == NULL)
		return;

	if (!CAMEL_IS_FOLDER (input->destination)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "Bad folder passed to fetch_mail");
		return;
	}

	data->empty = FALSE;
	camel_object_ref (CAMEL_OBJECT (input->destination));
}

static void
do_fetch_mail (gpointer in_data, gpointer op_data, CamelException * ex)
{
	fetch_mail_input_t *input = (fetch_mail_input_t *) in_data;
	fetch_mail_data_t *data = (fetch_mail_data_t *) op_data;

	CamelFolder *search_folder = NULL;

	/* If using IMAP, don't do anything... */

	if (!strncmp (input->source_url, "imap:", 5)) {
		data->empty = FALSE;
		return;
	}

	if (input->destination == NULL) {
		input->destination = mail_tool_get_local_inbox (ex);

		if (input->destination == NULL)
			return;
	}

	search_folder =
		mail_tool_fetch_mail_into_searchable (input->source_url, 
						      input->keep_on_server, ex);

	if (search_folder == NULL) {
		/* This happens with an IMAP source and on error 
		 * and on "no new mail"
		 */
		camel_object_unref (CAMEL_OBJECT (input->destination));
		input->destination = NULL;
		data->empty = TRUE;
		return;
	}

	mail_tool_camel_lock_up ();
	if (camel_folder_get_message_count (search_folder) == 0) {
		data->empty = TRUE;
	} else {
		mail_tool_filter_contents_into (search_folder, input->destination,
						TRUE,
						input->hook_func, input->hook_data,
						ex);
		data->empty = FALSE;
	}
	mail_tool_camel_lock_down ();

	camel_object_unref (CAMEL_OBJECT (search_folder));
}

static void
cleanup_fetch_mail (gpointer in_data, gpointer op_data, CamelException * ex)
{
	fetch_mail_input_t *input = (fetch_mail_input_t *) in_data;
	fetch_mail_data_t *data = (fetch_mail_data_t *) op_data;

	if (data->empty && !camel_exception_is_set (ex)) {
		GtkWidget *dialog;

		dialog = gnome_ok_dialog ("There is no new mail.");
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
mail_do_fetch_mail (const gchar * source_url, gboolean keep_on_server,
		    CamelFolder * destination,
		    gpointer hook_func, gpointer hook_data)
{
	fetch_mail_input_t *input;

	input = g_new (fetch_mail_input_t, 1);
	input->source_url = g_strdup (source_url);
	input->keep_on_server = keep_on_server;
	input->destination = destination;
	input->hook_func = hook_func;
	input->hook_data = hook_data;

	mail_operation_queue (&op_fetch_mail, input, TRUE);
}

/* ** SEND MAIL *********************************************************** */

typedef struct send_mail_input_s
{
	gchar *xport_uri;
	CamelMimeMessage *message;
	gchar *from;

	/* If done_folder != NULL, will add done_flags to
	 * the flags of the message done_uid in done_folder. */

	CamelFolder *done_folder;
	char *done_uid;
	guint32 done_flags;

	GtkWidget *composer;
}
send_mail_input_t;

static gchar *describe_send_mail (gpointer in_data, gboolean gerund);
static void setup_send_mail (gpointer in_data, gpointer op_data,
			     CamelException * ex);
static void do_send_mail (gpointer in_data, gpointer op_data,

			  CamelException * ex);
static void cleanup_send_mail (gpointer in_data, gpointer op_data,
			       CamelException * ex);

static gchar *
describe_send_mail (gpointer in_data, gboolean gerund)
{
	send_mail_input_t *input = (send_mail_input_t *) in_data;

	if (gerund) {
		if (input->message->subject && input->message->subject[0])
			return g_strdup_printf ("Sending \"%s\"",
						input->message->subject);
		else
			return
				g_strdup
				("Sending a message without a subject");
	} else {
		if (input->message->subject && input->message->subject[0])
			return g_strdup_printf ("Send \"%s\"",
						input->message->subject);
		else
			return g_strdup ("Send a message without a subject");
	}
}

static void
setup_send_mail (gpointer in_data, gpointer op_data, CamelException * ex)
{
	send_mail_input_t *input = (send_mail_input_t *) in_data;

	if (!input->xport_uri) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No transport URI specified for send_mail operation.");
		return;
	}

	if (!CAMEL_IS_MIME_MESSAGE (input->message)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No message specified for send_mail operation.");
		return;
	}

	if (input->from == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No from address specified for send_mail operation.");
		return;
	}

	/* NOTE THE EARLY EXIT!! */

	if (input->done_folder == NULL) {
		camel_object_ref (CAMEL_OBJECT (input->message));
		gtk_object_ref (GTK_OBJECT (input->composer));
		gtk_widget_hide (GTK_WIDGET (input->composer));
		return;
	}

	if (!CAMEL_IS_FOLDER (input->done_folder)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "Bad done_folder specified for send_mail operation.");
		return;
	}

	if (input->done_uid == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No done_uid specified for send_mail operation.");
		return;
	}

	if (!GTK_IS_WIDGET (input->composer)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No composer specified for send_mail operation.");
		return;
	}

	camel_object_ref (CAMEL_OBJECT (input->message));
	camel_object_ref (CAMEL_OBJECT (input->done_folder));
	gtk_object_ref (GTK_OBJECT (input->composer));
	gtk_widget_hide (GTK_WIDGET (input->composer));
}

static void
do_send_mail (gpointer in_data, gpointer op_data, CamelException * ex)
{
	send_mail_input_t *input = (send_mail_input_t *) in_data;
	CamelTransport *xport;

	mail_tool_camel_lock_up ();
	camel_mime_message_set_from (input->message, input->from);

	camel_medium_add_header (CAMEL_MEDIUM (input->message), "X-Mailer",
				 "Evolution (Developer Preview)");
	camel_mime_message_set_date (input->message,
				     CAMEL_MESSAGE_DATE_CURRENT, 0);

	xport = camel_session_get_transport (session, input->xport_uri, ex);
	mail_tool_camel_lock_down ();
	if (camel_exception_is_set (ex))
		return;

	mail_tool_send_via_transport (xport, CAMEL_MEDIUM (input->message),
				      ex);

	if (camel_exception_is_set (ex))
		return;

	if (input->done_folder) {
		guint32 set;

		mail_tool_camel_lock_up ();
		set = camel_folder_get_message_flags (input->done_folder,
						      input->done_uid);
		camel_folder_set_message_flags (input->done_folder,
						input->done_uid,
						input->done_flags, ~set);
		mail_tool_camel_lock_down ();
	}
}

static void
cleanup_send_mail (gpointer in_data, gpointer op_data, CamelException * ex)
{
	send_mail_input_t *input = (send_mail_input_t *) in_data;

	camel_object_unref (CAMEL_OBJECT (input->message));
	if (input->done_folder)
		camel_object_unref (CAMEL_OBJECT (input->done_folder));

	g_free (input->from);
	g_free (input->xport_uri);
	g_free (input->done_uid);

	if (!camel_exception_is_set (ex))
		gtk_widget_destroy (input->composer);
	else
		gtk_widget_show (input->composer);
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
		   CamelMimeMessage * message,
		   const char * from,
		   CamelFolder * done_folder,
		   const char *done_uid,
		   guint32 done_flags, GtkWidget * composer)
{
	send_mail_input_t *input;

	input = g_new (send_mail_input_t, 1);
	input->xport_uri = g_strdup (xport_uri);
	input->message = message;
	input->from = g_strdup (from);
	input->done_folder = done_folder;
	input->done_uid = g_strdup (done_uid);
	input->done_flags = done_flags;
	input->composer = composer;

	mail_operation_queue (&op_send_mail, input, TRUE);
}

/* ** EXPUNGE FOLDER ****************************************************** */

static gchar *describe_expunge_folder (gpointer in_data, gboolean gerund);
static void setup_expunge_folder (gpointer in_data, gpointer op_data,
				  CamelException * ex);
static void do_expunge_folder (gpointer in_data, gpointer op_data,
			       CamelException * ex);
static void cleanup_expunge_folder (gpointer in_data, gpointer op_data,
				    CamelException * ex);

static gchar *
describe_expunge_folder (gpointer in_data, gboolean gerund)
{
	CamelFolder *f = CAMEL_FOLDER (in_data);

	if (gerund)
		return g_strdup_printf ("Expunging \"%s\"", mail_tool_get_folder_name (f));
	else
		return g_strdup_printf ("Expunge \"%s\"", mail_tool_get_folder_name (f));
}

static void
setup_expunge_folder (gpointer in_data, gpointer op_data, CamelException * ex)
{
	if (!CAMEL_IS_FOLDER (in_data)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No folder is selected to be expunged");
		return;
	}

	camel_object_ref (CAMEL_OBJECT (in_data));
}

static void
do_expunge_folder (gpointer in_data, gpointer op_data, CamelException * ex)
{
	mail_tool_camel_lock_up ();
	camel_folder_expunge (CAMEL_FOLDER (in_data), ex);
	mail_tool_camel_lock_down ();
}

static void
cleanup_expunge_folder (gpointer in_data, gpointer op_data,
			CamelException * ex)
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
mail_do_expunge_folder (CamelFolder * folder)
{
	mail_operation_queue (&op_expunge_folder, folder, FALSE);
}

/* ** REFILE MESSAGES ***************************************************** */

typedef struct refile_messages_input_s
{
	CamelFolder *source;
	GPtrArray *uids;
	gchar *dest_uri;
}
refile_messages_input_t;

static gchar *describe_refile_messages (gpointer in_data, gboolean gerund);
static void setup_refile_messages (gpointer in_data, gpointer op_data,
				   CamelException * ex);
static void do_refile_messages (gpointer in_data, gpointer op_data,
				CamelException * ex);
static void cleanup_refile_messages (gpointer in_data, gpointer op_data,
				     CamelException * ex);

static gchar *
describe_refile_messages (gpointer in_data, gboolean gerund)
{
	refile_messages_input_t *input = (refile_messages_input_t *) in_data;

	if (gerund)
		return
			g_strdup_printf
			("Moving messages from \"%s\" into \"%s\"",
			 mail_tool_get_folder_name (input->source), input->dest_uri);
	else
		return
			g_strdup_printf
			("Move messages from \"%s\" into \"%s\"",
			 mail_tool_get_folder_name (input->source), input->dest_uri);
}

static void
setup_refile_messages (gpointer in_data, gpointer op_data,
		       CamelException * ex)
{
	refile_messages_input_t *input = (refile_messages_input_t *) in_data;

	if (!CAMEL_IS_FOLDER (input->source)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No source folder to refile messages from specified.");
		return;
	}

	if (input->uids == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No messages to refile have been specified.");
		return;
	}

	if (input->dest_uri == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No URI to refile to has been specified.");
		return;
	}

	camel_object_ref (CAMEL_OBJECT (input->source));
}

static void
do_refile_messages (gpointer in_data, gpointer op_data, CamelException * ex)
{
	refile_messages_input_t *input = (refile_messages_input_t *) in_data;
	CamelFolder *dest;
	gint i;

	dest = mail_tool_uri_to_folder (input->dest_uri, ex);
	if (camel_exception_is_set (ex))
		return;

	mail_tool_camel_lock_up ();
	camel_folder_freeze (input->source);
	camel_folder_freeze (dest);

	for (i = 0; i < input->uids->len; i++) {
		camel_folder_move_message_to (input->source,
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
cleanup_refile_messages (gpointer in_data, gpointer op_data,
			 CamelException * ex)
{
	refile_messages_input_t *input = (refile_messages_input_t *) in_data;

	camel_object_unref (CAMEL_OBJECT (input->source));
	g_free (input->dest_uri);
	g_ptr_array_free (input->uids, TRUE);
}

static const mail_operation_spec op_refile_messages = {
	describe_refile_messages,
	0,
	setup_refile_messages,
	do_refile_messages,
	cleanup_refile_messages
};

void
mail_do_refile_messages (CamelFolder * source, GPtrArray * uids,
			 gchar * dest_uri)
{
	refile_messages_input_t *input;

	input = g_new (refile_messages_input_t, 1);
	input->source = source;
	input->uids = uids;
	input->dest_uri = g_strdup (dest_uri);

	mail_operation_queue (&op_refile_messages, input, TRUE);
}

/* ** FLAG MESSAGES ******************************************************* */

typedef struct flag_messages_input_s
{
	CamelFolder *source;
	GPtrArray *uids;
	gboolean invert;
	guint32 mask;
	guint32 set;
}
flag_messages_input_t;

static gchar *describe_flag_messages (gpointer in_data, gboolean gerund);
static void setup_flag_messages (gpointer in_data, gpointer op_data,
				 CamelException * ex);
static void do_flag_messages (gpointer in_data, gpointer op_data,
			      CamelException * ex);
static void cleanup_flag_messages (gpointer in_data, gpointer op_data,
				   CamelException * ex);

static gchar *
describe_flag_messages (gpointer in_data, gboolean gerund)
{
	flag_messages_input_t *input = (flag_messages_input_t *) in_data;

	/* FIXME: change based on flags being applied? */

	if (gerund)
		return g_strdup_printf ("Marking messages in folder \"%s\"",
					mail_tool_get_folder_name (input->source));
	else
		return g_strdup_printf ("Mark messages in folder \"%s\"",
					mail_tool_get_folder_name (input->source));
}

static void
setup_flag_messages (gpointer in_data, gpointer op_data, CamelException * ex)
{
	flag_messages_input_t *input = (flag_messages_input_t *) in_data;

	if (!CAMEL_IS_FOLDER (input->source)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No source folder to flag messages from specified.");
		return;
	}

	if (input->uids == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No messages to flag have been specified.");
		return;
	}

	camel_object_ref (CAMEL_OBJECT (input->source));
}

static void
do_flag_messages (gpointer in_data, gpointer op_data, CamelException * ex)
{
	flag_messages_input_t *input = (flag_messages_input_t *) in_data;
	gint i;

	mail_tool_camel_lock_up ();
	camel_folder_freeze (input->source);
	mail_tool_camel_lock_down ();

	for (i = 0; i < input->uids->len; i++) {
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

		g_free (input->uids->pdata[i]);
	}

	mail_tool_camel_lock_up ();
	camel_folder_thaw (input->source);
	mail_tool_camel_lock_down ();
}

static void
cleanup_flag_messages (gpointer in_data, gpointer op_data,
		       CamelException * ex)
{
	flag_messages_input_t *input = (flag_messages_input_t *) in_data;

	camel_object_unref (CAMEL_OBJECT (input->source));
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
mail_do_flag_messages (CamelFolder * source, GPtrArray * uids,
		       gboolean invert,
		       guint32 mask, guint32 set)
{
	flag_messages_input_t *input;

	input = g_new (flag_messages_input_t, 1);
	input->source = source;
	input->uids = uids;
	input->invert = invert;
	input->mask = mask;
	input->set = set;

	mail_operation_queue (&op_flag_messages, input, TRUE);
}

/* ** SCAN SUBFOLDERS ***************************************************** */

typedef struct scan_subfolders_input_s
{
	gchar *source_uri;
	gboolean add_INBOX;
	EvolutionStorage *storage;
}
scan_subfolders_input_t;

typedef struct scan_subfolders_folderinfo_s
{
	char *path;
	char *uri;
}
scan_subfolders_folderinfo_t;

typedef struct scan_subfolders_op_s
{
	GPtrArray *new_folders;
}
scan_subfolders_op_t;

static gchar *describe_scan_subfolders (gpointer in_data, gboolean gerund);
static void setup_scan_subfolders (gpointer in_data, gpointer op_data,
				   CamelException * ex);
static void do_scan_subfolders (gpointer in_data, gpointer op_data,
				CamelException * ex);
static void cleanup_scan_subfolders (gpointer in_data, gpointer op_data,
				     CamelException * ex);

static gchar *
describe_scan_subfolders (gpointer in_data, gboolean gerund)
{
	scan_subfolders_input_t *input = (scan_subfolders_input_t *) in_data;

	if (gerund)
		return g_strdup_printf ("Scanning folders in \"%s\"",
					input->source_uri);
	else
		return g_strdup_printf ("Scan folders in \"%s\"",
					input->source_uri);
}

static void
setup_scan_subfolders (gpointer in_data, gpointer op_data,
		       CamelException * ex)
{
	scan_subfolders_input_t *input = (scan_subfolders_input_t *) in_data;
	scan_subfolders_op_t *data = (scan_subfolders_op_t *) op_data;

	if (!input->source_uri) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No source uri to scan subfolders from was provided.");
		return;
	}

	if (!EVOLUTION_IS_STORAGE (input->storage)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No storage to scan subfolders into was provided.");
		return;
	}

	gtk_object_ref (GTK_OBJECT (input->storage));
	data->new_folders = g_ptr_array_new ();
}

static void
do_scan_subfolders (gpointer in_data, gpointer op_data, CamelException * ex)
{
	scan_subfolders_input_t *input = (scan_subfolders_input_t *) in_data;
	scan_subfolders_op_t *data = (scan_subfolders_op_t *) op_data;

	scan_subfolders_folderinfo_t *info;
	GPtrArray *lsub;
	CamelFolder *folder;
	int i;
	char *splice;

	if (input->source_uri[strlen (input->source_uri) - 1] == '/')
		splice = "";
	else
		splice = "/";

	folder = mail_tool_get_root_of_store (input->source_uri, ex);
	if (camel_exception_is_set (ex))
		return;

	mail_tool_camel_lock_up ();

	/* we need a way to set the namespace */
	lsub = camel_folder_get_subfolder_names (folder);

	mail_tool_camel_lock_down ();

	if (input->add_INBOX) {
		info = g_new (scan_subfolders_folderinfo_t, 1);
		info->path = g_strdup ("/INBOX");
		info->uri =
			g_strdup_printf ("%s%sINBOX", input->source_uri,
					 splice);
		g_ptr_array_add (data->new_folders, info);
	}

	for (i = 0; i < lsub->len; i++) {
		info = g_new (scan_subfolders_folderinfo_t, 1);
		info->path = g_strdup_printf ("/%s", (char *) lsub->pdata[i]);
		info->uri =
			g_strdup_printf ("%s%s%s", input->source_uri, splice,
					 info->path);
		g_ptr_array_add (data->new_folders, info);
	}

	camel_folder_free_subfolder_names (folder, lsub);
	camel_object_unref (CAMEL_OBJECT (folder));
}

static void
cleanup_scan_subfolders (gpointer in_data, gpointer op_data,
			 CamelException * ex)
{
	scan_subfolders_input_t *input = (scan_subfolders_input_t *) in_data;
	scan_subfolders_op_t *data = (scan_subfolders_op_t *) op_data;

	int i;

	for (i = 0; i < data->new_folders->len; i++) {
		scan_subfolders_folderinfo_t *info;

		info = data->new_folders->pdata[i];
		evolution_storage_new_folder (input->storage,
					      info->path,
					      "mail",
					      info->uri, "(No description)");
		g_free (info->path);
		g_free (info->uri);
		g_free (info);
	}

	g_ptr_array_free (data->new_folders, TRUE);
	gtk_object_unref (GTK_OBJECT (input->storage));
	g_free (input->source_uri);
}

static const mail_operation_spec op_scan_subfolders = {
	describe_scan_subfolders,
	sizeof (scan_subfolders_op_t),
	setup_scan_subfolders,
	do_scan_subfolders,
	cleanup_scan_subfolders
};

void
mail_do_scan_subfolders (const gchar * source_uri, gboolean add_INBOX,
			 EvolutionStorage * storage)
{
	scan_subfolders_input_t *input;

	input = g_new (scan_subfolders_input_t, 1);
	input->source_uri = g_strdup (source_uri);
	input->add_INBOX = add_INBOX;
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

static gchar *describe_attach_message (gpointer in_data, gboolean gerund);
static void setup_attach_message (gpointer in_data, gpointer op_data,
				  CamelException * ex);
static void do_attach_message (gpointer in_data, gpointer op_data,
			       CamelException * ex);
static void cleanup_attach_message (gpointer in_data, gpointer op_data,
				    CamelException * ex);

static gchar *
describe_attach_message (gpointer in_data, gboolean gerund)
{
	attach_message_input_t *input = (attach_message_input_t *) in_data;

	if (gerund)
		return
			g_strdup_printf
			("Attaching messages from folder \"%s\"",
			 mail_tool_get_folder_name (input->folder));
	else
		return g_strdup_printf ("Attach messages from \"%s\"",
					mail_tool_get_folder_name (input->folder));
}

static void
setup_attach_message (gpointer in_data, gpointer op_data, CamelException * ex)
{
	attach_message_input_t *input = (attach_message_input_t *) in_data;

	if (!input->uid) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No UID specified to attach.");
		return;
	}

	if (!CAMEL_IS_FOLDER (input->folder)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No folder to fetch the message from specified.");
		return;
	}

	if (!E_IS_MSG_COMPOSER (input->composer)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No message composer from specified.");
		return;
	}

	camel_object_ref (CAMEL_OBJECT (input->folder));
	gtk_object_ref (GTK_OBJECT (input->composer));
}

static void
do_attach_message (gpointer in_data, gpointer op_data, CamelException * ex)
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
			CamelException * ex)
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
mail_do_attach_message (CamelFolder * folder, const char *uid,
			EMsgComposer * composer)
{
	attach_message_input_t *input;

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

static gchar *describe_forward_messages (gpointer in_data, gboolean gerund);
static void setup_forward_messages (gpointer in_data, gpointer op_data,
				    CamelException * ex);
static void do_forward_messages (gpointer in_data, gpointer op_data,
				 CamelException * ex);
static void cleanup_forward_messages (gpointer in_data, gpointer op_data,
				      CamelException * ex);

static gchar *
describe_forward_messages (gpointer in_data, gboolean gerund)
{
	forward_messages_input_t *input =

		(forward_messages_input_t *) in_data;

	if (gerund) {
		if (input->basis->subject)
			return g_strdup_printf ("Forwarding messages \"%s\"",
						input->basis->subject);
		else
			return
				g_strdup_printf
				("Forwarding a message without a subject");
	} else {
		if (input->basis->subject)
			return g_strdup_printf ("Forward message \"%s\"",
						input->basis->subject);
		else
			return
				g_strdup_printf
				("Forward a message without a subject");
	}
}

static void
setup_forward_messages (gpointer in_data, gpointer op_data,
			CamelException * ex)
{
	forward_messages_input_t *input =

		(forward_messages_input_t *) in_data;

	if (!input->uids) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No UIDs specified to attach.");
		return;
	}

	if (!CAMEL_IS_MIME_MESSAGE (input->basis)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No basic message to forward was specified.");
		return;
	}

	if (!CAMEL_IS_FOLDER (input->source)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No folder to fetch the messages from specified.");
		return;
	}

	if (!E_IS_MSG_COMPOSER (input->composer)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No message composer from specified.");
		return;
	}

	camel_object_ref (CAMEL_OBJECT (input->basis));
	camel_object_ref (CAMEL_OBJECT (input->source));
	gtk_object_ref (GTK_OBJECT (input->composer));
}

static void
do_forward_messages (gpointer in_data, gpointer op_data, CamelException * ex)
{
	forward_messages_input_t *input =

		(forward_messages_input_t *) in_data;
	forward_messages_data_t *data = (forward_messages_data_t *) op_data;

	CamelMimeMessage *message;
	CamelMimePart *part;
	int i;

	data->parts = g_ptr_array_new ();

	mail_tool_camel_lock_up ();
	for (i = 0; i < input->uids->len; i++) {
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
					     "Failed to generate mime part from "
					     "message while generating forwarded message.");
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
			  CamelException * ex)
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
mail_do_forward_message (CamelMimeMessage * basis,
			 CamelFolder * source,
			 GPtrArray * uids, EMsgComposer * composer)
{
	forward_messages_input_t *input;

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

static gchar *describe_load_folder (gpointer in_data, gboolean gerund);
static void setup_load_folder (gpointer in_data, gpointer op_data,
			       CamelException * ex);
static void do_load_folder (gpointer in_data, gpointer op_data,
			    CamelException * ex);
static void cleanup_load_folder (gpointer in_data, gpointer op_data,
				 CamelException * ex);

static gchar *
describe_load_folder (gpointer in_data, gboolean gerund)
{
	load_folder_input_t *input = (load_folder_input_t *) in_data;

	if (gerund) {
		return g_strdup_printf ("Loading \"%s\"", input->url);
	} else {
		return g_strdup_printf ("Load \"%s\"", input->url);
	}
}

static void
setup_load_folder (gpointer in_data, gpointer op_data, CamelException * ex)
{
	load_folder_input_t *input = (load_folder_input_t *) in_data;

	if (!IS_FOLDER_BROWSER (input->fb)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No folder browser specified to load into.");
		return;
	}

	if (!input->url) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No URL to load was specified.");
		return;
	}

	gtk_object_ref (GTK_OBJECT (input->fb));

	if (input->fb->uri)
		g_free (input->fb->uri);

	input->fb->uri = input->url;
}

static void
do_load_folder (gpointer in_data, gpointer op_data, CamelException * ex)
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
cleanup_load_folder (gpointer in_data, gpointer op_data, CamelException * ex)
{
	load_folder_input_t *input = (load_folder_input_t *) in_data;

	gtk_widget_set_sensitive (GTK_WIDGET (input->fb->search_entry),
				  camel_folder_has_search_capability (input->
								      fb->
								      folder));
	gtk_widget_set_sensitive (GTK_WIDGET (input->fb->search_menu),
				  camel_folder_has_search_capability (input->
								      fb->
								      folder));

	message_list_set_folder (input->fb->message_list, input->fb->folder);

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
mail_do_load_folder (FolderBrowser * fb, const char *url)
{
	load_folder_input_t *input;

	input = g_new (load_folder_input_t, 1);
	input->fb = fb;
	input->url = g_strdup (url);

	mail_operation_queue (&op_load_folder, input, TRUE);
}

/* ** CREATE FOLDER ******************************************************* */

typedef struct create_folder_input_s
{
	Evolution_ShellComponentListener listener;
	char *uri;
	char *type;
}
create_folder_input_t;

typedef struct create_folder_data_s
{
	Evolution_ShellComponentListener_Result result;
}
create_folder_data_t;

static gchar *describe_create_folder (gpointer in_data, gboolean gerund);
static void setup_create_folder (gpointer in_data, gpointer op_data,
				 CamelException * ex);
static void do_create_folder (gpointer in_data, gpointer op_data,
			      CamelException * ex);
static void cleanup_create_folder (gpointer in_data, gpointer op_data,
				   CamelException * ex);

static gchar *
describe_create_folder (gpointer in_data, gboolean gerund)
{
	create_folder_input_t *input = (create_folder_input_t *) in_data;

	if (gerund) {
		return g_strdup_printf ("Creating \"%s\"", input->uri);
	} else {
		return g_strdup_printf ("Create \"%s\"", input->uri);
	}
}

static void
setup_create_folder (gpointer in_data, gpointer op_data, CamelException * ex)
{
	create_folder_input_t *input = (create_folder_input_t *) in_data;

	if (input->listener == CORBA_OBJECT_NIL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "Invalid listener passed to create_folder");
		return;
	}

	if (input->uri == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "Invalid url passed to create_folder");
		return;
	}

	if (input->type == NULL) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No type passed to create_folder");
		return;
	}

	/* FIXME: reference listener somehow? */
}

static void
do_create_folder (gpointer in_data, gpointer op_data, CamelException * ex)
{
	create_folder_input_t *input = (create_folder_input_t *) in_data;
	create_folder_data_t *data = (create_folder_data_t *) op_data;

	CamelFolder *folder;
	gchar *camel_url;

	if (strcmp (input->type, "mail") != 0)
		data->result =
			Evolution_ShellComponentListener_UNSUPPORTED_TYPE;
	else {
		camel_url = g_strdup_printf ("mbox://%s", input->uri);
		folder = mail_tool_get_folder_from_urlname (camel_url,
							    "mbox", TRUE, ex);
		g_free (camel_url);

		if (!camel_exception_is_set (ex)) {
			camel_object_unref (CAMEL_OBJECT (folder));
			data->result = Evolution_ShellComponentListener_OK;
		} else {
			data->result =
				Evolution_ShellComponentListener_INVALID_URI;
		}
	}
}

static void
cleanup_create_folder (gpointer in_data, gpointer op_data,
		       CamelException * ex)
{
	create_folder_input_t *input = (create_folder_input_t *) in_data;
	create_folder_data_t *data = (create_folder_data_t *) op_data;

	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	Evolution_ShellComponentListener_report_result (input->listener,
							data->result, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
				     "Exception while reporting result to shell "
				     "component listener.");
	CORBA_exception_free (&ev);

	/* FIXME: unref listener somehow? */

	g_free (input->uri);
	g_free (input->type);
}

static const mail_operation_spec op_create_folder = {
	describe_create_folder,
	sizeof (create_folder_data_t),
	setup_create_folder,
	do_create_folder,
	cleanup_create_folder
};

void
mail_do_create_folder (const Evolution_ShellComponentListener listener,
		       const char *uri, const char *type)
{
	create_folder_input_t *input;

	input = g_new (create_folder_input_t, 1);
	input->listener = listener;
	input->uri = g_strdup (uri);
	input->type = g_strdup (type);

	mail_operation_queue (&op_create_folder, input, FALSE);
}

/* ** SYNC FOLDER ********************************************************* */

static gchar *describe_sync_folder (gpointer in_data, gboolean gerund);
static void setup_sync_folder (gpointer in_data, gpointer op_data,
			       CamelException * ex);
static void do_sync_folder (gpointer in_data, gpointer op_data,
			    CamelException * ex);
static void cleanup_sync_folder (gpointer in_data, gpointer op_data,
				 CamelException * ex);

static gchar *
describe_sync_folder (gpointer in_data, gboolean gerund)
{
	CamelFolder *f = CAMEL_FOLDER (in_data);

	if (gerund) {
		return g_strdup_printf ("Synchronizing \"%s\"", mail_tool_get_folder_name (f));
	} else {
		return g_strdup_printf ("Synchronize \"%s\"", mail_tool_get_folder_name (f));
	}
}

static void
setup_sync_folder (gpointer in_data, gpointer op_data, CamelException * ex)
{
	if (!CAMEL_IS_FOLDER (in_data)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No folder is selected to be synced");
		return;
	}

	camel_object_ref (CAMEL_OBJECT (in_data));
}

static void
do_sync_folder (gpointer in_data, gpointer op_data, CamelException * ex)
{
	mail_tool_camel_lock_up ();
	camel_folder_sync (CAMEL_FOLDER (in_data), FALSE, ex);
	mail_tool_camel_lock_down ();
}

static void
cleanup_sync_folder (gpointer in_data, gpointer op_data, CamelException * ex)
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
mail_do_sync_folder (CamelFolder * folder)
{
	mail_operation_queue (&op_sync_folder, folder, FALSE);
}

/* ** DISPLAY MESSAGE ***************************************************** */

typedef struct display_message_input_s
{
	MessageList *ml;
	gchar *uid;
	 gint (*timeout) (gpointer);
}
display_message_input_t;

typedef struct display_message_data_s
{
	CamelMimeMessage *msg;
}
display_message_data_t;

static gchar *describe_display_message (gpointer in_data, gboolean gerund);
static void setup_display_message (gpointer in_data, gpointer op_data,
				   CamelException * ex);
static void do_display_message (gpointer in_data, gpointer op_data,
				CamelException * ex);
static void cleanup_display_message (gpointer in_data, gpointer op_data,
				     CamelException * ex);

static gchar *
describe_display_message (gpointer in_data, gboolean gerund)
{
	display_message_input_t *input = (display_message_input_t *) in_data;

	if (gerund) {
		if (input->uid)
			return g_strdup_printf ("Displaying message UID \"%s\"",
						input->uid);
		else
			return g_strdup ("Clearing message display");
	} else {
		if (input->uid)
			return g_strdup_printf ("Display message UID \"%s\"",
						input->uid);
		else
			return g_strdup ("Clear message dispaly");
	}
}

static void
setup_display_message (gpointer in_data, gpointer op_data,
		       CamelException * ex)
{
	display_message_input_t *input = (display_message_input_t *) in_data;
	display_message_data_t *data = (display_message_data_t *) op_data;

	if (!IS_MESSAGE_LIST (input->ml)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "Invalid message list passed to display_message");
		return;
	}

	if (!input->timeout) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No timeout callback passed to display_message");
		return;
	}

	data->msg = NULL;
	gtk_object_ref (GTK_OBJECT (input->ml));
}

static void
do_display_message (gpointer in_data, gpointer op_data, CamelException * ex)
{
	display_message_input_t *input = (display_message_input_t *) in_data;
	display_message_data_t *data = (display_message_data_t *) op_data;

	if (input->uid == NULL) {
		data->msg = NULL;
		return;
	}

	data->msg = camel_folder_get_message (input->ml->folder,
					      input->uid, ex);
}

static void
cleanup_display_message (gpointer in_data, gpointer op_data,
			 CamelException * ex)
{
	display_message_input_t *input = (display_message_input_t *) in_data;
	display_message_data_t *data = (display_message_data_t *) op_data;

	MailDisplay *md = input->ml->parent_folder_browser->mail_display;

	if (data->msg == NULL) {
		mail_display_set_message (md, NULL);
	} else {
		if (input->ml->seen_id)
			gtk_timeout_remove (input->ml->seen_id);

		mail_display_set_message (md, CAMEL_MEDIUM (data->msg));
		camel_object_unref (CAMEL_OBJECT (data->msg));

		input->ml->seen_id =
			gtk_timeout_add (1500, input->timeout, input->ml);
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
mail_do_display_message (MessageList * ml, const char *uid,
			 gint (*timeout) (gpointer))
{
	display_message_input_t *input;

	input = g_new (display_message_input_t, 1);
	input->ml = ml;
	input->uid = g_strdup (uid);
	input->timeout = timeout;

	mail_operation_queue (&op_display_message, input, FALSE);
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

static gchar *describe_edit_messages (gpointer in_data, gboolean gerund);
static void setup_edit_messages (gpointer in_data, gpointer op_data,
				  CamelException * ex);
static void do_edit_messages (gpointer in_data, gpointer op_data,
			       CamelException * ex);
static void cleanup_edit_messages (gpointer in_data, gpointer op_data,
				    CamelException * ex);

static gchar *
describe_edit_messages (gpointer in_data, gboolean gerund)
{
	edit_messages_input_t *input = (edit_messages_input_t *) in_data;

	if (gerund)
		return g_strdup_printf
			("Opening messages from folder \"%s\"",
			 mail_tool_get_folder_name (input->folder));
	else
		return g_strdup_printf ("Open messages from \"%s\"",
					mail_tool_get_folder_name (input->folder));
}

static void
setup_edit_messages (gpointer in_data, gpointer op_data, CamelException * ex)
{
	edit_messages_input_t *input = (edit_messages_input_t *) in_data;

	if (!input->uids) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No UIDs specified to edit.");
		return;
	}

	if (!CAMEL_IS_FOLDER (input->folder)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No folder to fetch the messages from specified.");
		return;
	}

	camel_object_ref (CAMEL_OBJECT (input->folder));
}

static void
do_edit_messages (gpointer in_data, gpointer op_data, CamelException * ex)
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
			CamelException * ex)
{
	edit_messages_input_t *input = (edit_messages_input_t *) in_data;
	edit_messages_data_t *data = (edit_messages_data_t *) op_data;

	int i;

	for (i = 0; i < data->messages->len; i++) {
		GtkWidget *composer;

		composer = e_msg_composer_new_with_message (data->messages->pdata[i]);

		if (input->signal)
			gtk_signal_connect (GTK_OBJECT (composer), "send", 
					    input->signal, NULL);

		gtk_widget_show (composer);

		camel_object_unref (CAMEL_OBJECT (data->messages->pdata[i]));
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
mail_do_edit_messages (CamelFolder * folder, GPtrArray *uids,
		       GtkSignalFunc signal)
{
	edit_messages_input_t *input;

	input = g_new (edit_messages_input_t, 1);
	input->folder = folder;
	input->uids = uids;
	input->signal = signal;

	mail_operation_queue (&op_edit_messages, input, TRUE);
}

/* ** SETUP DRAFTBOX ****************************************************** */

static gchar *describe_setup_draftbox (gpointer in_data, gboolean gerund);
static void noop_setup_draftbox (gpointer in_data, gpointer op_data,
				  CamelException * ex);
static void do_setup_draftbox (gpointer in_data, gpointer op_data,
			       CamelException * ex);

static gchar *
describe_setup_draftbox (gpointer in_data, gboolean gerund)
{
	if (gerund)
		return g_strdup_printf ("Loading Draftbox");
	else
		return g_strdup_printf ("Load Draftbox");
}

static void
noop_setup_draftbox (gpointer in_data, gpointer op_data, CamelException * ex)
{
}

static void
do_setup_draftbox (gpointer in_data, gpointer op_data, CamelException * ex)
{
	extern CamelFolder *drafts_folder;
	gchar *url;

	url = g_strdup_printf ("mbox://%s/local/Drafts", evolution_dir);
	drafts_folder = mail_tool_get_folder_from_urlname (url, "mbox", TRUE, ex);
	g_free (url);
}

/*
 *static void
 *cleanup_setup_draftbox (gpointer in_data, gpointer op_data,
 *			CamelException * ex)
 *{
 *}
 */

static const mail_operation_spec op_setup_draftbox = {
	describe_setup_draftbox,
	0,
	noop_setup_draftbox,
	do_setup_draftbox,
	noop_setup_draftbox
};

void
mail_do_setup_draftbox (void)
{
	mail_operation_queue (&op_setup_draftbox, NULL, FALSE);
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

static gchar *describe_view_messages (gpointer in_data, gboolean gerund);
static void setup_view_messages (gpointer in_data, gpointer op_data,
				  CamelException * ex);
static void do_view_messages (gpointer in_data, gpointer op_data,
			       CamelException * ex);
static void cleanup_view_messages (gpointer in_data, gpointer op_data,
				    CamelException * ex);

static gchar *
describe_view_messages (gpointer in_data, gboolean gerund)
{
	view_messages_input_t *input = (view_messages_input_t *) in_data;

	if (gerund)
		return g_strdup_printf
			("Viewing messages from folder \"%s\"",
			 mail_tool_get_folder_name (input->folder));
	else
		return g_strdup_printf ("View messages from \"%s\"",
					mail_tool_get_folder_name (input->folder));
}

static void
setup_view_messages (gpointer in_data, gpointer op_data, CamelException * ex)
{
	view_messages_input_t *input = (view_messages_input_t *) in_data;

	if (!input->uids) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No UIDs specified to view.");
		return;
	}

	if (!CAMEL_IS_FOLDER (input->folder)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No folder to fetch the messages from specified.");
		return;
	}

	if (!IS_FOLDER_BROWSER (input->fb)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No folder browser was specified.");
		return;
	}


	camel_object_ref (CAMEL_OBJECT (input->folder));
	gtk_object_ref (GTK_OBJECT (input->fb));
}

static void
do_view_messages (gpointer in_data, gpointer op_data, CamelException * ex)
{
	view_messages_input_t *input = (view_messages_input_t *) in_data;
	view_messages_data_t *data = (view_messages_data_t *) op_data;

	int i;

	data->messages = g_ptr_array_new ();

	for (i = 0; i < input->uids->len; i++) {
		CamelMimeMessage *message;

		mail_tool_camel_lock_up ();
		message = camel_folder_get_message (input->folder, input->uids->pdata[i], ex);
		mail_tool_camel_lock_down ();

		g_ptr_array_add (data->messages, message);
	}
}

static void
cleanup_view_messages (gpointer in_data, gpointer op_data,
		       CamelException * ex)
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
mail_do_view_messages (CamelFolder * folder, GPtrArray *uids,
		       FolderBrowser *fb)
{
	view_messages_input_t *input;

	input = g_new (view_messages_input_t, 1);
	input->folder = folder;
	input->uids = uids;
	input->fb = fb;

	mail_operation_queue (&op_view_messages, input, TRUE);
}
