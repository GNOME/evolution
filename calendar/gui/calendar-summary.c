/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* calendar-summary.c
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

#include <gnome.h>

#include <executive-summary/evolution-services/executive-summary-component.h>
#include "cal-util/cal-component.h"
#include "calendar-model.h"

#include "calendar-summary.h"

typedef struct {
	ExecutiveSummaryComponent *component;
	CalClient *client;
} CalSummary;

static char *
generate_html_summary (CalSummary *summary)
{
	GList *uids, *l;
	char *ret_html, *tmp;
	
	ret_html = g_strdup ("<ul>");
	
	uids = cal_client_get_uids (summary->client, CALOBJ_TYPE_ANY);
	for (l = uids; l; l = l->next){
		CalComponent *comp;
		CalComponentText text;
		CalClientGetStatus status;
		char *uid;
		char *tmp2;
		
		uid = l->data;
		status = cal_client_get_object (summary->client, uid, &comp);
		if (status != CAL_CLIENT_GET_SUCCESS)
			continue;
		
		cal_component_get_summary (comp, &text);
		
		tmp2 = g_strdup_printf ("<li>%s</li>", text.value);
		
		tmp = ret_html;
		ret_html = g_strconcat (ret_html, tmp2, NULL);
		g_free (tmp);
		g_free (tmp2);
	}
	
	cal_obj_uid_list_free (uids);
	
	tmp = ret_html;
	ret_html = g_strconcat (ret_html, "</ul>", NULL);
	g_free (tmp);
	
	return ret_html;
}

static void
cal_loaded (CalClient *client,
	    CalClientLoadStatus status,
	    CalSummary *summary)
{
	char *html;
	
	if (status == CAL_CLIENT_LOAD_SUCCESS) {
		html = generate_html_summary (summary);
		executive_summary_component_update (summary->component, html);
		g_free (html);
	} else {
		g_print ("Error loading %d\n", status);
		executive_summary_component_update (summary->component, "");
	}
}

char *
create_summary_view (ExecutiveSummaryComponent *component,
		     char **title,
		     char **icon,
		     void *closure)
{
	char *ret_html;
	char *evoldir;
	char *calfile;
	CalSummary *summary;
	gboolean result;
	
	evoldir = (char *) closure;
	
	/* strdup the title and icon */
	*title = g_strdup ("Things to do");
	*icon = g_strdup ("evolution-tasks.png");
	
	summary = g_new (CalSummary, 1);
	summary->component = component;
	
	calfile = g_strdup_printf ("%s/local/Calendar/calendar.ics", evoldir);
	g_print ("calfile: %s\n", calfile);
	summary->client = cal_client_new ();
	
	result = cal_client_load_calendar (summary->client, calfile);
	if (!result) {
		g_warning ("%s: Could not load %s", __FUNCTION__, calfile);
		return "";
	}
	
	gtk_signal_connect (GTK_OBJECT (summary->client), "cal_loaded",
			    GTK_SIGNAL_FUNC (cal_loaded), summary);
	
	return g_strdup ("");
}
