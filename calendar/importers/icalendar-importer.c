/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar importer component
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 2001  Ximian, Inc.
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

#include <sys/types.h>
#include <fcntl.h>
#include <gtk/gtksignal.h>
#include <cal-client.h>
#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>
#include "evolution-calendar-importer.h"

typedef struct {
	CalClient *client;
	EvolutionImporter *importer;
	icalcomponent *icalcomp;
} ICalImporter;

static void
importer_destroy_cb (GtkObject *object, gpointer user_data)
{
	ICalImporter *ici = (ICalImporter *) user_data;

	g_return_if_fail (ici != NULL);

	gtk_object_unref (GTK_OBJECT (ici->client));
	if (ici->icalcomp != NULL)
		icalcomponent_free (ici->icalcomp);
	g_free (ici);
}

static gboolean
support_format_fn (EvolutionImporter *importer,
		   const char *filename,
		   void *closure)
{
	int fd;
	GString *str;
	icalcomponent *icalcomp;
	int n;
	char buffer[2049];
	gboolean ret = TRUE;

	/* read file contents */
	fd = open (filename, O_RDONLY);
	if (fd == -1)
		return FALSE;

	str = g_string_new ("");
	while (1) {
		memset (buffer, 0, sizeof(buffer));
		n = read (fd, buffer, sizeof (buffer) - 1);
		if (n > 0) {
			str = g_string_append (str, buffer);
		}
		else if (n == 0)
			break;
		else {
			ret = FALSE;
			break;
		}
	}

	close (fd);

	/* parse the file */
	if (ret) {
		icalcomp = icalparser_parse_string (str->str);
		if (icalcomp)
			icalcomponent_free (icalcomp);
		else
			ret = FALSE;
	}

	g_string_free (str, TRUE);

	return ret;
}

static gboolean
load_file_fn (EvolutionImporter *importer,
	      const char *filename,
	      const char *folderpath,
	      void *closure)
{
	int fd;
	GString *str;
	icalcomponent *icalcomp;
	int n;
	char buffer[2049];
	char *uri_str;
	gboolean ret = TRUE;
	ICalImporter *ici = (ICalImporter *) closure;

	g_return_val_if_fail (ici != NULL, FALSE);

	if (folderpath == NULL || *folderpath == '\0')
		uri_str = g_strdup_printf ("%s/evolution/local/Calendar/calendar.ics",
					   g_get_home_dir ());
	else {
		char *name;
		char *parent;

		name = strrchr (folderpath, '/');
		if (name == NULL || name == folderpath) {
			parent = g_strdup ("evolution/local/");
			if (folderpath[0] == '/')
				name = folderpath + 1;
			else
				name = folderpath;
		}
		else {
			name += 1;
			parent = g_strdup ("evolution/local/Calendar/subfolders/");
		}
		uri_str = g_strdup_printf ("%s/%s%s/calendar.ics", g_get_home_dir (),
					   parent, name);
	}

	/* read file contents */
	fd = open (filename, O_RDONLY);
	if (fd == -1)
		return FALSE;

	str = g_string_new ("");
	while (1) {
		memset (buffer, 0, sizeof(buffer));
		n = read (fd, buffer, sizeof (buffer) - 1);
		if (n > 0) {
			str = g_string_append (str, buffer);
		}
		else if (n == 0)
			break;
		else {
			ret = FALSE;
			break;
		}
	}

	close (fd);

	/* parse the file */
	if (ret) {
		icalcomp = icalparser_parse_string (str->str);
		if (icalcomp) {
			if (!cal_client_open_calendar (ici->client, uri_str, TRUE))
				ret = FALSE;
			else
				ici->icalcomp = icalcomp;
		}
		else
			ret = FALSE;
	}

	g_string_free (str, TRUE);
	g_free (uri_str);

	return ret;
}

static void
process_item_fn (EvolutionImporter *importer,
		 CORBA_Object listener,
		 void *closure,
		 CORBA_Environment *ev)
{
	CalClientLoadState state;
	ICalImporter *ici = (ICalImporter *) closure;

	g_return_if_fail (ici != NULL);
	g_return_if_fail (IS_CAL_CLIENT (ici->client));
	g_return_if_fail (ici->icalcomp != NULL);

	state = cal_client_get_load_state (ici->client);
	if (state == CAL_CLIENT_LOAD_LOADING) {
		GNOME_Evolution_ImporterListener_notifyResult (
			listener,
			GNOME_Evolution_ImporterListener_NOT_READY,
			TRUE, ev);
		return;
	}
	else if (state != CAL_CLIENT_LOAD_LOADED) {
		GNOME_Evolution_ImporterListener_notifyResult (
			listener,
			GNOME_Evolution_ImporterListener_UNSUPPORTED_OPERATION,
			FALSE, ev);
		return;
	}

	/* import objects into the given client */
	if (!cal_client_update_objects (ici->client, ici->icalcomp)) {
		g_warning ("Could not update objects");
		GNOME_Evolution_ImporterListener_notifyResult (
			listener,
			GNOME_Evolution_ImporterListener_BAD_DATA,
			FALSE, ev);
	}
	else {
		GNOME_Evolution_ImporterListener_notifyResult (
			listener,
			GNOME_Evolution_ImporterListener_OK,
			FALSE, ev);
	}
}

BonoboObject *
ical_importer_new (void)
{
	ICalImporter *ici;

	ici = g_new0 (ICalImporter, 1);
	ici->client = cal_client_new ();
	ici->icalcomp = NULL;
	ici->importer = evolution_importer_new (support_format_fn,
						load_file_fn,
						process_item_fn,
						NULL,
						ici);
	gtk_signal_connect (GTK_OBJECT (ici->importer), "destroy",
			    GTK_SIGNAL_FUNC (importer_destroy_cb), ici);

	return BONOBO_OBJECT (ici->importer);
}
