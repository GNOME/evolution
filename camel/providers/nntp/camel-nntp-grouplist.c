/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-grouplist.c : getting/updating the list of newsgroups on the server. */

/* 
 * Author : Chris Toshok <toshok@ximian.com> 
 *
 * Copyright (C) 2000 Ximian .
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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
#include <errno.h>
#include <string.h>

#include "camel-exception.h"
#include "camel-nntp-grouplist.h"
#include "camel-nntp-resp-codes.h"

static CamelNNTPGroupList *
camel_nntp_get_grouplist_from_server (CamelNNTPStore *store, CamelException *ex)
{
	int status;
	gboolean done = FALSE;
	CamelNNTPGroupList *list;

	CAMEL_NNTP_STORE_LOCK(store);
	status = camel_nntp_command (store, ex, NULL, &line, "LIST");

	if (status != NNTP_LIST_FOLLOWS) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not get group list from server."));
		return NULL;
	}

	list = g_new0 (CamelNNTPGroupList, 1);
	list->time = time (NULL);

	while (!done) {
		char *line;

		if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (store), &line, ex) < 0) {
			list->group_list = g_list_reverse(list->group_list);
			return list;
		}

		if (*line == '.') {
			done = TRUE;
		}
		else {
			CamelNNTPGroupListEntry *entry = g_new (CamelNNTPGroupListEntry, 1);
			char **split_line = g_strsplit (line, " ", 4);

			entry->group_name = g_strdup (split_line[0]);
			entry->high = atoi (split_line[1]);
			entry->low = atoi (split_line[2]);

			g_strfreev (split_line);
			
			list->group_list = g_list_prepend (list->group_list, entry);
		}
	}
	CAMEL_NNTP_STORE_UNLOCK(store);

	list->group_list = g_list_reverse(list->group_list);
	return list;
}

static CamelNNTPGroupList*
camel_nntp_get_grouplist_from_file (CamelNNTPStore *store, CamelException *ex)
{
	gchar *root_dir = camel_nntp_store_get_toplevel_dir(CAMEL_NNTP_STORE(store));
	gchar *grouplist_file = g_strdup_printf ("%s/grouplist", root_dir);
	CamelNNTPGroupList *list;
	FILE *fp;
	char buf[300];
	unsigned long time;

	g_free (root_dir);
	fp = fopen (grouplist_file, "r");
	g_free (grouplist_file);

	if (fp == NULL) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Unable to load grouplist file for %s: %s"),
				      CAMEL_SERVICE(store)->url->host,
				      strerror(errno));
		return NULL;
	}

	/* read the time */
	if (!fgets (buf, sizeof (buf), fp)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Unable to load grouplist file for %s: %s"),
				      CAMEL_SERVICE(store)->url->host,
				      strerror(errno));
		fclose (fp);
		return NULL;
	}


	list = g_new0 (CamelNNTPGroupList, 1);
	list->store = store;
	sscanf (buf, "%lu", &time);
	list->time = time;

	while (fgets (buf, sizeof (buf), fp)) {
		CamelNNTPGroupListEntry *entry = g_new (CamelNNTPGroupListEntry, 1);
		char **split_line = g_strsplit (buf, " ", 4);

		entry->group_name = g_strdup (split_line[0]);
		entry->high = atoi (split_line[1]);
		entry->low = atoi (split_line[2]);

		g_strfreev (split_line);

		list->group_list = g_list_prepend (list->group_list, entry);
	}

	fclose (fp);

	list->group_list = g_list_reverse(list->group_list);
	return list;
}

static void
save_entry (CamelNNTPGroupListEntry *entry, FILE *fp)
{
	fprintf (fp, "%s %d %d\n", entry->group_name, entry->low, entry->high);
}

void
camel_nntp_grouplist_save (CamelNNTPGroupList *group_list, CamelException *ex)
{
	FILE *fp;
	gchar *root_dir = camel_nntp_store_get_toplevel_dir(CAMEL_NNTP_STORE(group_list->store));
	gchar *grouplist_file = g_strdup_printf ("%s/grouplist", root_dir);

	g_free (root_dir);
	fp = fopen (grouplist_file, "w");
	g_free (grouplist_file);

	if (fp == NULL) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      _("Unable to save grouplist file for %s: %s"),
				      CAMEL_SERVICE(group_list->store)->url->host,
				      strerror(errno));
		return;
	}

	fprintf (fp, "%lu\n", (long)group_list->time);

	g_list_foreach (group_list->group_list, (GFunc)save_entry, fp);

	fclose (fp);
}

static void
free_entry (CamelNNTPGroupListEntry *entry, void *data)
{
	g_free (entry->group_name);
	g_free (entry);
}

void
camel_nntp_grouplist_free (CamelNNTPGroupList *group_list)
{
	g_return_if_fail (group_list);

	g_list_foreach (group_list->group_list, (GFunc)free_entry, NULL);

	g_free (group_list);
}

CamelNNTPGroupList*
camel_nntp_grouplist_fetch (CamelNNTPStore *store, CamelException *ex)
{
	CamelNNTPGroupList *list;

	list = camel_nntp_get_grouplist_from_file (store, ex);

	printf ("camel_nntp_get_grouplist_from_file returned %p\n", list);

	if (!list) {
		camel_exception_clear (ex);

		list = camel_nntp_get_grouplist_from_server (store, ex);

		if (!list) {
			camel_nntp_grouplist_free (list);
		}
		else {
			list->store = store;
			camel_nntp_grouplist_save (list, ex);
			return list;
		}
	}

	return list;
}

gint
camel_nntp_grouplist_update (CamelNNTPGroupList *group_list, CamelException *ex)
{
	return 0;
}
