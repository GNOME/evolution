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

#include "mail.h"		/* YUCK FIXME */
#include "mail-tools.h"
#include "mail-ops.h"
#include <gal/widgets/e-gui-utils.h>
#include "mail-local-storage.h"

#include <executive-summary/evolution-services/executive-summary-component.h>

typedef struct {
	ExecutiveSummaryComponent *component;
	CamelFolder *folder;
	
	int mailread, mailunread;
	char *html;
} MailSummary;

/* Temporary functions to create the summary
   FIXME: Need TigerT's designs :) */
static void
folder_changed_cb (CamelObject *folder,
		   gpointer event_data,
		   gpointer user_data)
{
	MailSummary *summary;
	char *ret_html, *str1, *str2;
	int mailread, mailunread;
	
	summary = (MailSummary *)user_data;

	mailread = camel_folder_get_message_count (CAMEL_FOLDER (folder));
	mailunread = camel_folder_get_unread_message_count (CAMEL_FOLDER (folder));
	
	str1 = g_strdup_printf (_("There %s %d %s."), 
				(mailread == 1) ? _("is"): _("are"),
				mailread,
				(mailread == 1) ? _("message"): _("messages"));
	str2 = g_strdup_printf (_("There %s %d unread %s."),
				(mailunread == 1) ? _("is"): _("are"),
				mailunread,
				(mailunread == 1) ? _("message"): _("messages"));
	
	ret_html = g_strdup_printf ("<table><tr><td><img src=\"evolution-inbox-mini.png\"></td><td>%s</td></tr><tr><td><img src=\"evolution-inbox-mini.png\"></td><td>%s</td></tr></table>", str1, str2);
	g_free (str1);
	g_free (str2);

	executive_summary_component_update (summary->component, ret_html);
	g_free (ret_html);
}

char *
create_summary_view (ExecutiveSummaryComponent *component,
		     char **title,
		     void *closure)
{
	char *str1, *str2, *ret_html;
	int mailread, mailunread;
	CamelFolder *folder;
	CamelException *ex;
	MailSummary *summary;

	ex = camel_exception_new ();
	folder = mail_tool_get_local_inbox (ex);
	
	/* Strdup the title */
	*title = g_strdup ("Inbox:");

	mailread = camel_folder_get_message_count (folder);
	mailunread = camel_folder_get_unread_message_count (folder);

	str1 = g_strdup_printf (_("There %s %d %s."), 
			       (mailread == 1) ? _("is"): _("are"),
			       mailread,
			       (mailread == 1) ? _("message"): _("messages"));
	str2 = g_strdup_printf (_("There %s %d unread %s."),
				(mailunread == 1) ? _("is"): _("are"),
				mailunread,
				(mailunread == 1) ? _("message"): _("messages"));

	ret_html = g_strdup_printf ("<table><tr><td><img src=\"evolution-inbox-mini.png\"></td><td>%s</td></tr><tr><td><img src=\"evolution-inbox-mini.png\"></td><td>%s</td></tr></table>", str1, str2);
	g_free (str1);
	g_free (str2);

	summary = g_new (MailSummary, 1);
	summary->folder = folder;
	summary->html = ret_html;
	summary->mailread = mailread;
	summary->mailunread = mailunread;
	summary->component = component;

	camel_object_hook_event (folder, "folder_changed",
				 (CamelObjectEventHookFunc) folder_changed_cb,
				 summary);
	camel_object_hook_event (folder, "message_changed",
				 (CamelObjectEventHookFunc) folder_changed_cb,
				 summary);
	return ret_html;
}
