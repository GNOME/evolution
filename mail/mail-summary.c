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

#include "mail-local-storage.h"

#include <executive-summary/evolution-services/executive-summary-component.h>

typedef struct {
	ExecutiveSummaryComponent *component;
	CamelFolder *folder;
	
	int mail, unread;
	char *html;
} MailSummary;

#define SUMMARY_IN() g_print ("IN: %s: %d\n", __FUNCTION__, __LINE__);
#define SUMMARY_OUT() g_print ("OUT: %s: %d\n", __FUNCTION__, __LINE__);

static int queue_len = 0;

#define MAIN_READER main_compipe[0]
#define MAIN_WRITER main_compipe[1]
#define DISPATCH_READER dispatch_compipe[0]
#define DISPATCH_WRITER dispatch_compipe[1]

static int main_compipe[2] = {-1, -1};
static int dispatch_compipe[2] = {-1, -1};

GIOChannel *summary_chan_reader = NULL;

static void do_changed (MailSummary *summary);

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

/* Temporary functions to create the summary
   FIXME: Need TigerT's designs :) */
static void
do_changed (MailSummary *summary)
{
	char *ret_html, *str1;
	int mail = summary->mail;

	str1 = g_strdup_printf (_("<b>Inbox:</b> %d/%d"), 
				summary->unread, mail);
	
	ret_html = g_strdup_printf ("<table><tr><td><img src=\"evolution-inbox-mini.png\"></td><td>%s</td></tr></table>", str1);
	g_free (str1);

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
	int mail;

	summary = (MailSummary *) user_data;
	/* Put the summary data onto a pipe */

	mail = camel_folder_get_message_count (folder);
	summary->unread = camel_folder_get_unread_message_count (folder);
	summary->mail = mail;

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
	int mail;

	summary = (MailSummary *)user_data;

	summary->unread = camel_folder_get_unread_message_count (folder);
	summary->mail = camel_folder_get_message_count (folder);

	write (MAIN_WRITER, summary, sizeof (MailSummary));
	queue_len++;
	
	return;
}

char *
create_summary_view (ExecutiveSummaryComponent *component,
		     char **title,
		     void *closure)
{
	char *str1, *ret_html;
	int mailread, unread;
	CamelFolder *folder;
	CamelException *ex;
	MailSummary *summary;

	/* Strdup the title */
	*title = g_strdup ("Inbox:");

	mail_tool_camel_lock_up ();
	ex = camel_exception_new ();
	folder = mail_tool_get_local_inbox (ex);
	
	mailread = camel_folder_get_message_count (folder);
	unread = camel_folder_get_unread_message_count (folder);
	mail_tool_camel_lock_down ();

	str1 = g_strdup_printf (_("<b>Inbox:</b>%d/%d"), 
				unread, mailread);

	ret_html = g_strdup_printf ("<table><tr><td><img src=\"evolution-inbox-mini.png\"></td><td>%s</td></tr></table>", str1);
	g_free (str1);

	summary = g_new (MailSummary, 1);
	summary->folder = folder;
	summary->html = ret_html;
	summary->mail = mailread;
	summary->unread = unread;
	summary->component = component;

	check_compipes ();

	mail_tool_camel_lock_up ();
	camel_object_hook_event (folder, "folder_changed",
				 (CamelObjectEventHookFunc) folder_changed_cb,
				 summary);
	camel_object_hook_event (folder, "message_changed",
				 (CamelObjectEventHookFunc) message_changed_cb,
				 summary);
	mail_tool_camel_lock_down ();
	return ret_html;
}
