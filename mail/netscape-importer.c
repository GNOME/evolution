/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* netscape-importer.c
 *
 * 
 * Authors: 
 *    Iain Holmes  <iain@ximian.com>
 *
 * Copyright 2001 Ximian, Inc. (http://www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <glib.h>
#include <gnome.h>

#include "mail-importer.h"
static char *nsmail_dir = NULL;

/*#define SUPER_IMPORTER_DEBUG*/
#ifdef SUPER_IMPORTER_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#if 0
typedef struct {
	MailImporter importer;

	int num;
	CamelMimeParser *mp;
} NetscapeImporter;

static void
netscape_clean_up (void)
{
	g_free (nsmail_dir);
	nsmail_dir = NULL;
}

static gboolean
netscape_can_import (void)
{
	char *nsprefs;
	FILE *prefs_handle;

	nsprefs = gnome_util_prepend_user_home (".netscape/preferences.js");
	prefs_handle = fopen (nsprefs, "r");
	g_free (nsprefs);

	if (prefs_handle == NULL) {
		d(g_warning ("No .netscape/preferences.js"));
		return FALSE;
	}

	/* Find the user mail dir */
	while (1) {
		char line[4096];

		fgets (line, 4096, prefs_handle);
		if (line == NULL) {
			d(g_warning ("No mail.directory entry"));
			fclose (prefs_handle);
			return FALSE;
		}

		if (strstr (line, "mail.directory") != NULL) {
			char *sep, *start, *end;
			/* Found the line */
			
			sep = strchr (line, ',');
			if (sep == NULL) {
				d(g_warning ("Bad line %s", line));
				fclose (prefs_handle);
				return FALSE;
			}

			start = strchr (sep, '\"') + 1;
			if (start == NULL) {
				d(g_warning ("Bad line %s", line));
				fclose (prefs_handle);
				return FALSE;
			}

			end = strrchr (sep, '\"');
			if (end == NULL) {
				d(g_warning ("Bad line %s", line));
				fclose (prefs_handle);
				return FALSE;
			}
			
			nsmail_dir = g_strndup (start, end - start);
			d(g_warning ("Got nsmail_dir: %s", nsmail_dir));
			fclose (prefs_handle);
			return TRUE;
		}
	}
}

static void
netscape_import_file (const char *parent,
		      const char *dirname,
		      const char *filename)
{
	char *summary, *summarypath;
	
	/* Check that the file is a netscape mbox. 
	   It should have an associated .summary file */
	summary = g_strdup_printf (".%s.summary", filename);
	summarypath = g_concat_dir_and_file (dirname, summary);
	if (!g_file_exists (summarypath)) {
		d(g_warning ("%s does not exist.\nIgnoring %s", summary,
			     filename));
		g_free (summary);
		g_free (summarypath);
		return;
	}

	g_free (summary);
	g_free (summarypath);

	/* Do import */
	mail_importer_create_folder (parent, filename, "mail", NULL);
	g_print ("Importing %s as %s\n", parent, filename);
}

static void
scan_dir (NetscapeImporter *importer,
	  char *parent,
	  const char *dirname)
{
	DIR *nsmail;
	struct stat buf;
	struct dirent *current;

	nsmail = opendir (dirname);
	if (nsmail == NULL) {
		d(g_warning ("Could not open %s\nopendir returned: %s", 
			     dirname, g_strerror (errno)));
		return;
	}

	current = readdir (nsmail);
	while (current) {
		char *fullname;

		/* Ignore things which start with . 
		   which should be ., .., and the summaries. */
		if (current->d_name[0] =='.') {
			current = readdir (nsmail);
			continue;
		}

		fullname = g_concat_dir_and_file (dirname, current->d_name);
		if (stat (fullname, &buf) == -1) {
			d(g_warning ("Could not stat %s\nstat returned:%s",
				     fullname, g_strerror (errno)));
			current = readdir (nsmail);
			g_free (fullname);
			continue;
		}

		if (S_ISREG (buf.st_mode)) {
			d(g_print ("File: %s\n", fullname));
			netscape_import_file (importer, parent, dirname, 
					      current->d_name);
		} else if (S_ISDIR (buf.st_mode)) {
			char *ext;
			d(g_print ("Directory: %s\n", fullname));
			
			ext = strrchr (current->d_name, '.');
			if (ext && strcmp (ext + 1, "sbd") == 0) {
				/* Strip the .sbd */
				if (parent == NULL) {
					parent = g_strndup (current->d_name, 
							    ext - current->d_name);
				} else {
					char *part;
					char *tmp;

					part = g_strndup (current->d_name,
							  ext - current->d_name);
					tmp = parent;
					parent = g_concat_dir_and_file (parent,
									part);
					g_free (tmp);
					g_free (part);
				}
				
				scan_dir (importer, parent, fullname);
			}
		}
		
		g_free (fullname);
		current = readdir (nsmail);
	}
}

static void
netscape_create_structure (void)
{
	DIR *nsmail;
	struct dirent *current;
	NetscapeImporter *importer;

	g_return_if_fail (nsmail_dir != NULL);

	importer = g_new0 (NetscapeImporter, 1);

	g_print ("Creating structure\n"
		 "------------------\n");
	scan_dir (importer, g_strdup ("/"), nsmail_dir);
	g_print ("------------------\n");
}


#ifdef STANDALONE	
int
main (int argc,
      char **argv)
#else
int 
netscape_importer(void)
#endif
{
	gboolean found;

	g_print ("ISI - Iain's Super Importer\n");
	g_print ("Checking for Netscape mail:\t");
	found = netscape_can_import ();
	g_print ("%s", found ? "Found" : "Not found");
	
	if (found)
		g_print (" (%s)\n", nsmail_dir);
	else
		g_print ("\n");

	netscape_create_structure ();
}

BonoboObject *
mbox_factory_fn (BonoboGenericFactory *_factory,
		 void *closure)
{
	EvolutionImporter *importer;
	NetscapeImporter *netscape;

	netscape = g_new0 (NetscapeImporter, 1);
	importer = evolution_importer_new (support_format_fn,
					   load_file_fn,
					   process_item_fn, NULL, netscape);
}
#endif
