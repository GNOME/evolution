/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-summary.c
 *
 * Authors: Iain Holmes <iain@ximian.com>
 *
 * Copyright (C) 2000  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include <bonobo/bonobo-property-bag.h>

#include "camel.h"
#include "mail.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-vfolder.h"
#include "mail-summary.h"

#include "Evolution.h"
#include "evolution-storage.h"
#include "evolution-storage-listener.h"

#include "filter/vfolder-context.h"

#include <evolution-services/executive-summary-component.h>
#include <evolution-services/executive-summary-html-view.h>
#include <gal/widgets/e-unicode.h>

typedef struct {
	CamelFolder *folder;

	char *name;
	char *uri;
	int total, unread;
} FolderSummary;

typedef struct {
	BonoboObject *component;
	BonoboObject *view;
	EvolutionStorageListener *listener;

	GHashTable *folder_to_summary;
	FolderSummary **folders;
	int numfolders;

	char *title;
	char *icon;

	guint idle;
	gboolean in_summary;
} MailSummary;

#define SUMMARY_IN() g_print ("IN: %s: %d\n", G_GNUC_FUNCTION, __LINE__);
#define SUMMARY_OUT() g_print ("OUT: %s: %d\n", G_GNUC_FUNCTION, __LINE__);

static int queue_len = 0;

extern EvolutionStorage *vfolder_storage;

#define MAIN_READER main_compipe[0]
#define MAIN_WRITER main_compipe[1]
#define DISPATCH_READER dispatch_compipe[0]
#define DISPATCH_WRITER dispatch_compipe[1]

static int main_compipe[2] = {-1, -1};
static int dispatch_compipe[2] = {-1, -1};

GIOChannel *summary_chan_reader = NULL;

static gboolean do_changed (MailSummary *summary);

enum {
	PROPERTY_TITLE,
	PROPERTY_ICON
};

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

static void
folder_free (FolderSummary *folder)
{
	g_free (folder->name);
	g_free (folder->uri);
}

static void
summary_free (MailSummary *summary)
{
	int i;

	for (i = 0; i < summary->numfolders; i++){
		folder_free (summary->folders[i]);
	}

	g_free (summary->folders);
	g_free (summary->title);
	g_free (summary->icon);

	g_hash_table_destroy (summary->folder_to_summary);
}

static void
view_destroy_cb (MailSummary *summary, GObject *deadbeef)
{
	summary_free (summary);
	g_free (summary);
}

static char *
generate_html_summary (MailSummary *summary)
{
	char *ret_html = NULL, *tmp;
	FolderSummary *fs;
	int i;

	summary->in_summary = TRUE;
	/* Inbox first */
	fs = summary->folders[0];

	g_print ("%p: %p\n", fs, fs->name);
	g_print ("unread: %d\n", fs->unread);
	g_print ("total: %d\n", fs->total);

	tmp = g_strdup_printf ("<table><tr><td><b><a href=\"view://evolution:/local/Inbox\">%s</a>:</b>"
			       "<td align=\"right\">%d/%d</td></tr>",
			       fs->name, fs->unread, fs->total);

	ret_html = g_strdup (tmp);
	for (i = 1; i < summary->numfolders; i++) {
		char *tmp2; 

		fs = summary->folders[i];
		tmp2 = g_strdup_printf ("<tr><td><a href=\"view://%s\">%s</a>:</td>"
					"<td align=\"right\">%d/%d</td></tr>",
					fs->uri, fs->name, fs->unread, fs->total);

		tmp = ret_html;
		ret_html = g_strconcat (ret_html, tmp2, NULL);
		g_free (tmp);
		g_free (tmp2);
	}

	tmp = ret_html;
	ret_html = g_strconcat (ret_html, "</table>", NULL);
	g_free (tmp);

	summary->in_summary = FALSE;
	return ret_html;
}
	
static gboolean
do_changed (MailSummary *summary)
{
	char *ret_html;

	ret_html = generate_html_summary (summary);
	executive_summary_html_view_set_html(EXECUTIVE_SUMMARY_HTML_VIEW(summary->view), (const char *) ret_html);
	g_free (ret_html);

	summary->idle = 0;
	return TRUE;
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
		g_warning ("%s: Unknown folder", G_GNUC_FUNCTION);
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
		g_warning ("%s: Unknown folder.", G_GNUC_FUNCTION);
		return;
	}

	fs->unread = camel_folder_get_unread_message_count (fs->folder);
	fs->total = camel_folder_get_message_count (fs->folder);

	write (MAIN_WRITER, summary, sizeof (MailSummary));
	queue_len++;
	
	return;
}

static void
generate_folder_summaries (MailSummary *summary)
{
	int numfolders = 1; /* Always at least the Inbox */
	char *user, *system;
	FilterRule *rule;
	VfolderContext *context;
	FolderSummary *fs;
	CamelException *ex;
	int i;

	user = g_strdup_printf ("%s/vfolders.xml", mail_component_peek_base_directory (mail_component_peek ()));
	system = EVOLUTION_PRIVDATADIR "/vfoldertypes.xml";

	context = vfolder_context_new ();
	rule_context_load ((RuleContext *)context, system, user);
	g_free (user);

	rule = NULL;
	while ((rule = rule_context_next_rule ((RuleContext *)context, rule, NULL))){
		g_print ("rule->name: %s\n", rule->name);
		numfolders++;
	}

	if (summary->folders != NULL) {
		int i;
		
		for (i = 0; i < summary->numfolders; i++){
			folder_free (summary->folders[i]);
		}
		
		g_free (summary->folders);
	}

	summary->folders = g_new (FolderSummary *, numfolders);

	/* Inbox */
	fs = summary->folders[0] = g_new (FolderSummary, 1);
	fs->name = g_strdup ("Inbox");
	g_print ("%p: %s(%p)\n", fs, fs->name, fs->name);
	fs->uri = NULL;
	ex = camel_exception_new ();
	fs->folder = mail_tool_get_local_inbox (ex);

	fs->total = camel_folder_get_message_count (fs->folder);
	fs->unread = camel_folder_get_unread_message_count (fs->folder);
	camel_exception_free (ex);
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
		rule = rule_context_next_rule ((RuleContext *)context, rule, NULL);
		fs->name = g_strdup (rule->name);

		uri = g_strconcat ("vfolder:", rule->name, NULL);
		fs->folder = vfolder_uri_to_folder (uri, ex);
		fs->uri = g_strconcat ("evolution:/VFolders/", rule->name, NULL);
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
	}

	g_object_unref (context);
}

static void
get_property (BonoboPropertyBag *bag,
	      BonoboArg *arg,
	      guint arg_id,
	      CORBA_Environment *ev,
	      gpointer user_data)
{
	MailSummary *summary = (MailSummary *) user_data;

	switch (arg_id) {
	case PROPERTY_TITLE:
		BONOBO_ARG_SET_STRING (arg, summary->title);
		break;

	case PROPERTY_ICON:
		BONOBO_ARG_SET_STRING (arg, summary->icon);
		break;

	default:
		break;
	}
}

/* This code may play with the threads wrongly...
   if the mail component locks when you use the summary
   remove this define */
#define DETECT_NEW_VFOLDERS
#ifdef DETECT_NEW_VFOLDERS

/* Check that we can generate a new summary
   and keep coming back until we can. */
static gboolean
idle_check (gpointer data)
{
	MailSummary *summary = (MailSummary *) data;

	if (summary->in_summary == TRUE)
		return TRUE;

	generate_folder_summaries (summary);
	write (MAIN_WRITER, summary, sizeof (MailSummary));
	queue_len++;
	summary->idle = 0;
	
	return FALSE;
}

static void
new_folder_cb (EvolutionStorageListener *listener,
	       const char *path,
	       const GNOME_Evolution_Folder *folder,
	       MailSummary *summary)
{
	g_print ("New folder: %s\n", path);

	if (summary->idle == 0)
		summary->idle = g_idle_add ((GSourceFunc) idle_check, summary);
}

static void
removed_folder_cb (EvolutionStorageListener *listener,
		   const char *path,
		   MailSummary *summary)
{
	g_print ("Removed folder: %s\n", path);

	if (summary->idle == 0)
		summary->idle = g_idle_add ((GSourceFunc) idle_check, summary);
}
#endif

BonoboObject *
create_summary_view (ExecutiveSummaryComponentFactory *_factory,
		     void *closure)
{
	GNOME_Evolution_Storage corba_local_objref;
	GNOME_Evolution_StorageListener corba_object;
	CORBA_Environment ev;
	BonoboObject *component, *view;
	BonoboPropertyBag *bag;
	BonoboEventSource *event_source;
	MailSummary *summary;

	summary = g_new (MailSummary, 1);
	summary->folders = 0;
	summary->in_summary = FALSE;
	summary->folder_to_summary = g_hash_table_new (NULL, NULL);
	summary->title = e_utf8_from_locale_string (_("Mail Summary"));
	summary->icon = g_strdup ("envelope.png");
	summary->idle = 0;

	check_compipes ();

	component = executive_summary_component_new ();
	summary->component = component;

	event_source = bonobo_event_source_new ();

	view = executive_summary_html_view_new_full (event_source);
	bonobo_object_add_interface (component, view);
	summary->view = view;
	
	g_object_weak_ref ((GObject *) view, (GWeakNotify) view_destroy_cb, summary);

	bag = bonobo_property_bag_new_full (get_property, NULL, 
					    event_source, summary);
	bonobo_property_bag_add (bag,
				 "window_title", PROPERTY_TITLE,
				 BONOBO_ARG_STRING, NULL,
				 "The title of this component's window", 
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (bag,
				 "window_icon", PROPERTY_ICON,
				 BONOBO_ARG_STRING, NULL,
				 "The icon for this component's window", 
				 BONOBO_PROPERTY_READABLE);
	bonobo_object_add_interface (component, BONOBO_OBJECT(bag));

#ifdef DETECT_NEW_VFOLDERS 
	summary->listener = evolution_storage_listener_new ();
	g_signal_connect((summary->listener), "new_folder",
			    G_CALLBACK (new_folder_cb), summary);
	g_signal_connect((summary->listener), "removed_folder",
			    G_CALLBACK (removed_folder_cb), summary);

	corba_object = evolution_storage_listener_corba_objref (summary->listener);

	CORBA_exception_init (&ev);
	corba_local_objref = bonobo_object_corba_objref (BONOBO_OBJECT (vfolder_storage));

	GNOME_Evolution_Storage_addListener (corba_local_objref,
					     corba_object, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot add a listener to the vfolder storage.");
	}
	CORBA_exception_free (&ev);
#endif
	
	if (summary->idle == 0)
		summary->idle = g_idle_add ((GSourceFunc) idle_check, summary);

	return component;
}
