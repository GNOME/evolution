/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-summary.c
 *
 * Authors: Iain Holmes <iain@helixcode.com>
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo.h>

#include "camel.h"
#include <gnome.h>
#include "mail.h"		/* YUCK FIXME */
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-vfolder.h"

#include "Evolution.h"
#include "evolution-storage.h"

#include "mail-local-storage.h"

#include "filter/vfolder-context.h"

#include <executive-summary/evolution-services/executive-summary-component.h>

typedef struct {
	CamelFolder *folder;

	char *name;
	int total, unread;
} FolderSummary;

typedef struct {
	ExecutiveSummaryComponent *component;

	GHashTable *folder_to_summary;
	FolderSummary **folders;
	int numfolders;

	char *html;
} MailSummary;

#define SUMMARY_IN() g_print ("IN: %s: %d\n", __FUNCTION__, __LINE__);
#define SUMMARY_OUT() g_print ("OUT: %s: %d\n", __FUNCTION__, __LINE__);

static int queue_len = 0;

extern char *evolution_dir;

#define MAIN_READER main_compipe[0]
#define MAIN_WRITER main_compipe[1]
#define DISPATCH_READER dispatch_compipe[0]
#define DISPATCH_WRITER dispatch_compipe[1]

static int main_compipe[2] = {-1, -1};
static int dispatch_compipe[2] = {-1, -1};

GIOChannel *summary_chan_reader = NULL;

static void do_changed (MailSummary *summary);

/* Read a message from the pipe */
static gboolean
read_msg (GIOChannel *source,
	  GIOCondition condition,
	  gpointer user_data)
{
	MailSummary *summary;
	int size;

	summary = g_new0 (MailSummary, 1);
	g_io_channel_read (source, (gchar *) summary,
			   sizeof (MailSummary) / sizeof (gchar), &size);

	if (size != sizeof (MailSummary)) {
		g_warning (_("Incomplete message written on pipe!"));
		return TRUE;
	}

	do_changed (summary);
	g_free (summary);

	return TRUE;
}

/* check_compipes: */
static void
check_compipes (void)
{
	if (MAIN_READER < 0) {
		if (pipe (main_compipe) < 0) {
			g_warning ("Call to pipe failed");
			return;
		}

		summary_chan_reader = g_io_channel_unix_new (MAIN_READER);
		g_io_add_watch (summary_chan_reader, G_IO_IN, read_msg, NULL);
	}

	if (DISPATCH_READER < 0) {
		if (pipe (dispatch_compipe) < 0) {
			g_warning ("Call to pipe failed");
			return;
		}
	}
}

static char *
generate_html_summary (MailSummary *summary)
{
	char *ret_html = NULL, *tmp;
	FolderSummary *fs;
	int i;

	/* Inbox first */
	fs = summary->folders[0];

	tmp = g_strdup_printf ("<table><tr><td><b>%s:</b>"
			       "<td align=\"right\">%d/%d</td></tr>",
			       fs->name, fs->unread, fs->total);

	ret_html = g_strdup (tmp);
	for (i = 1; i < summary->numfolders; i++) {
		char *tmp2; 

		fs = summary->folders[i];
		tmp2 = g_strdup_printf ("<tr><td><ul><li>%s:</li></ul></td>"
					"<td align=\"right\">%d/%d</td></tr>",
					fs->name, fs->unread, fs->total);

		tmp = ret_html;
		ret_html = g_strconcat (ret_html, tmp2, NULL);
		g_free (tmp);
		g_free (tmp2);
	}

	tmp = ret_html;
	ret_html = g_strconcat (ret_html, "</table>", NULL);
	g_free (tmp);

	return ret_html;
}
	
static void
do_changed (MailSummary *summary)
{
	char *ret_html;

	ret_html = generate_html_summary (summary);
	executive_summary_component_update (summary->component, ret_html);
	g_free (ret_html);
}

/* These two callbacks are called from the Camel thread,
   which can't make any CORBA calls, or else ORBit locks up,
   and likewise the thread that can call ORBit, cannot call
   camel.

   So, when the callbacks are triggered, they generate a MailSummary
   structure and write this onto a pipe. The ORBit calling thread
   detects when something is written to the pipe and creates its own
   MailSummary structure, and calls the appropriate CORBA calls.

   Same theory as mail-threads.c, but a lot less complicated
   as there is only one way communication, and only one type of message
*/
static void
folder_changed_cb (CamelObject *folder,
		   gpointer event_data,
		   gpointer user_data)
{
	MailSummary *summary;
	FolderSummary *fs;

	summary = (MailSummary *) user_data;
	fs = g_hash_table_lookup (summary->folder_to_summary, folder);
	if (fs == NULL) {
		g_warning ("%s: Unknown folder", __FUNCTION__);
		return;
	}

	fs->total = camel_folder_get_message_count (fs->folder);
	fs->unread = camel_folder_get_unread_message_count (fs->folder);

	write (MAIN_WRITER, summary, sizeof (MailSummary));
	queue_len++;

	return;
}

static void
message_changed_cb (CamelObject *folder,
		    gpointer event_data,
		    gpointer user_data)
{
	MailSummary *summary;
	FolderSummary *fs;

	summary = (MailSummary *)user_data;
	fs = g_hash_table_lookup (summary->folder_to_summary, folder);
	if (fs == NULL) {
		g_warning ("%s: Unknown folder.", __FUNCTION__);
		return;
	}

	fs->unread = camel_folder_get_unread_message_count (fs->folder);
	fs->total = camel_folder_get_message_count (fs->folder);

	write (MAIN_WRITER, summary, sizeof (MailSummary));
	queue_len++;
	
	return;
}

static void
generate_folder_summarys (MailSummary *summary)
{
	int numfolders = 1; /* Always at least the Inbox */
	char *user, *system;
	FilterRule *rule;
	VfolderContext *context;
	FolderSummary *fs;
	CamelException *ex;
	int i;

	user = g_strdup_printf ("%s/vfolders.xml", evolution_dir);
	system = g_strdup_printf ("%s/evolution/vfoldertypes.xml", EVOLUTION_DATADIR);

	context = vfolder_context_new ();
	rule_context_load ((RuleContext *)context, system, user);
	g_free (user);
	g_free (system);

	rule = NULL;
	while ((rule = rule_context_next_rule ((RuleContext *)context, rule))){
		g_print ("rule->name: %s\n", rule->name);
		numfolders++;
	}

	summary->folders = g_new (FolderSummary *, numfolders);

	/* Inbox */
	fs = summary->folders[0] = g_new (FolderSummary, 1);
	fs->name = g_strdup ("Inbox");
	mail_tool_camel_lock_up ();
	ex = camel_exception_new ();
	fs->folder = mail_tool_get_local_inbox (ex);
	fs->total = camel_folder_get_message_count (fs->folder);
	fs->unread = camel_folder_get_unread_message_count (fs->folder);
	camel_exception_free (ex);
	mail_tool_camel_lock_down ();
	camel_object_hook_event (CAMEL_OBJECT (fs->folder), "folder_changed",
				 (CamelObjectEventHookFunc) folder_changed_cb,
				 summary);
	camel_object_hook_event (CAMEL_OBJECT (fs->folder), "message_changed",
				 (CamelObjectEventHookFunc) message_changed_cb,
				 summary);
	g_hash_table_insert (summary->folder_to_summary, fs->folder, fs);
	

	summary->numfolders = 1;

	for (i = 1, rule = NULL; i < numfolders; i++) {
		char *uri;

		ex = camel_exception_new ();
		fs = summary->folders[i] = g_new (FolderSummary, 1);
		rule = rule_context_next_rule ((RuleContext *)context, rule);
		fs->name = g_strdup (rule->name);

		uri = g_strconcat ("vfolder:", rule->name, NULL);
		mail_tool_camel_lock_up ();
		fs->folder = vfolder_uri_to_folder (uri, ex);
		g_free (uri);

		fs->total = camel_folder_get_message_count (fs->folder);
		fs->unread = camel_folder_get_unread_message_count (fs->folder);

		/* Connect to each folder */
		camel_object_hook_event (CAMEL_OBJECT (fs->folder), 
					 "folder_changed",
					 (CamelObjectEventHookFunc) folder_changed_cb,
					 summary);
		camel_object_hook_event (CAMEL_OBJECT (fs->folder), 
					 "message_changed",
					 (CamelObjectEventHookFunc) message_changed_cb,
					 summary);
		g_hash_table_insert (summary->folder_to_summary, fs->folder, fs);
		summary->numfolders++;

		camel_exception_free (ex);
		mail_tool_camel_lock_down ();
	}

	gtk_object_destroy (GTK_OBJECT (context));
}

char *
create_summary_view (ExecutiveSummaryComponent *component,
		     char **title,
		     char **icon,
		     void *closure)
{
	char *ret_html;
	MailSummary *summary;

	/* Strdup the title and icon */
	*title = g_strdup ("Mailbox summary");
	*icon = g_strdup ("envelope.png");

	summary = g_new (MailSummary, 1);
	summary->component = component;
	summary->folder_to_summary = g_hash_table_new (NULL, NULL);

	generate_folder_summarys (summary);

	ret_html = generate_html_summary (summary);

	check_compipes ();

	return ret_html;
}
