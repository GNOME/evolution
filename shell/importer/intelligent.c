/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* intelligent.c
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

#include <gnome.h>

#include <liboaf/liboaf.h>

#include "intelligent.h"
#include "GNOME_Evolution_Importer.h"

static void
start_importer (const char *iid)
{
	CORBA_Object importer;
	CORBA_Environment ev;
	CORBA_char *name;
	CORBA_char *message;
	CORBA_boolean can_run;

	GtkWidget *dialog, *label, *ask;
	gboolean dontaskagain;
	char *str, *prefix;

	if (iid == NULL || *iid == '\0')
		return;

	/* Check if we want to show this one again */
	prefix = g_strdup_printf ("=%s/evolution/config/Shell=/intelligent-importers/", gnome_util_user_home ());
	gnome_config_push_prefix (prefix);
	g_free (prefix);
	
	dontaskagain = gnome_config_get_bool (iid);
	gnome_config_pop_prefix ();

	if (dontaskagain)
		return;

	CORBA_exception_init (&ev);
	importer = oaf_activate_from_id ((char *) iid, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		g_warning ("Could not start %s", iid);
		return;
	}

	CORBA_exception_free (&ev);
	if (importer == CORBA_OBJECT_NIL) {
		g_warning ("Could not activate_component %s", iid);
		return;
	}

	CORBA_exception_init (&ev);
	can_run = GNOME_Evolution_IntelligentImporter_canImport (importer, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Could not get canImport(%s): %s", iid, CORBA_exception_id (&ev));
		CORBA_Object_release (importer, &ev);
		CORBA_exception_free (&ev);
		return;
	}
	CORBA_exception_free (&ev);
	
	if (can_run == FALSE) {
		return;
	}

	name = GNOME_Evolution_IntelligentImporter__get_importername (importer,
								      &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Could not get name(%s): %s", iid, CORBA_exception_id (&ev));
		CORBA_Object_release (importer, &ev);
		CORBA_exception_free (&ev);
		return;
	}
	message = GNOME_Evolution_IntelligentImporter__get_message (importer, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Could not get message(%s): %s", iid, CORBA_exception_id (&ev));
		CORBA_Object_release (importer, &ev);
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	dialog = gnome_dialog_new ("Import files",
				   GNOME_STOCK_BUTTON_YES, GNOME_STOCK_BUTTON_NO,
				   NULL);
	gtk_window_set_title (GTK_WINDOW (dialog), name);
	CORBA_free (name);

	label = gtk_label_new (message);
	CORBA_free (message);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, 
			    FALSE, FALSE, 0);
	gtk_widget_show (label);

	ask = gtk_check_button_new_with_label (_("Don't ask me again"));
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), ask,
			    FALSE, FALSE, 0);
	gtk_widget_show (ask);

	switch (gnome_dialog_run (GNOME_DIALOG (dialog))) {
	case 0:
		/* Yes */
		dontaskagain = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ask));
		prefix = g_strdup_printf ("=%s/evolution/config/Shell=/intelligent-importers/", gnome_util_user_home ());
		gnome_config_push_prefix (prefix);
		g_free (prefix);

		gnome_config_set_bool (iid, dontaskagain);
		gnome_config_sync ();
		gnome_config_drop_all ();

		gnome_config_pop_prefix ();

		gtk_object_destroy (GTK_OBJECT (dialog));
		while (gtk_events_pending ())
			gtk_main_iteration ();

		GNOME_Evolution_IntelligentImporter_importData (importer, &ev);
		break;
	case 1:
	case -1:
	default:
		/* No */
		dontaskagain = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ask));
		prefix = g_strdup_printf ("=%s/evolution/config/Shell=/intelligent-importers/", gnome_util_user_home ());
		gnome_config_push_prefix (prefix);
		g_free (prefix);
		
		gnome_config_set_bool (iid, dontaskagain);
		gnome_config_sync ();
		gnome_config_drop_all ();
		
		gnome_config_pop_prefix ();
		
		gtk_object_destroy (GTK_OBJECT (dialog));

		break;
	}

	CORBA_exception_init (&ev);
	CORBA_Object_release (importer, &ev);
	CORBA_exception_free (&ev);
}
	

static GList *
get_intelligent_importers (void)
{
	OAF_ServerInfoList *info_list;
	GList *iids_ret = NULL;
	CORBA_Environment ev;
	int i;

	CORBA_exception_init (&ev);
	info_list = oaf_query ("repo_ids.has ('IDL:GNOME/Evolution/IntelligentImporter:1.0')", NULL, &ev);
	CORBA_exception_free (&ev);

	for (i = 0; i < info_list->_length; i++) {
		const OAF_ServerInfo *info;
		
		info = info_list->_buffer + i;
		iids_ret = g_list_prepend (iids_ret, g_strdup (info->iid));
	}

	return iids_ret;
}

void
intelligent_importer_init (void)
{
	GList *importers, *l;

	importers = get_intelligent_importers ();
	if (importers == NULL)
		return; /* No intelligent importers. Easy :) */

	/* Loop through each importer, running it. */
	for (l = importers; l; l = l->next) {
		start_importer (l->data);
		g_free (l->data);
	}

	g_list_free (importers);
}

