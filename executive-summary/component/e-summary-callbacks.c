/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-callbacks.c
 *
 * Author: 
 *          Iain Holmes <iain@helixcode.com>
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

#include <gnome.h>
#include <bonobo.h>

#include <liboaf/liboaf.h>
#include <glade/glade.h>

#include "e-summary.h"

#include "Composer.h"

#define COMPOSER_IID "OAFIID:GNOME_Evolution_Mail_Composer"
typedef struct _PropertyData {
	ESummary *esummary;
	GnomePropertyBox *box;
	GladeXML *xml;
} PropertyData;

void
embed_service (GtkWidget *widget,
	       ESummary *esummary)
{
	char *required_interfaces[2] = {"IDL:GNOME/Evolution:Summary:ComponentFactory:1.0",
					NULL};
	char *obj_id;
	
	obj_id = bonobo_selector_select_id ("Select a service",
					    (const char **) required_interfaces);
	if (obj_id == NULL)
		return;

	e_summary_embed_service_from_id (esummary, obj_id);
}

void
new_mail (GtkWidget *widget,
	  ESummary *esummary)
{
	GNOME_Evolution_Composer_RecipientList *to, *cc, *bcc;
	CORBA_char *subject;
	CORBA_Object composer;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	composer = oaf_activate_from_id ((char *)COMPOSER_IID, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION || composer == NULL) {
		CORBA_exception_free (&ev);
		g_warning ("Unable to start composer component!");
		return;
	}
	CORBA_exception_free (&ev);

	to = GNOME_Evolution_Composer_RecipientList__alloc ();
	to->_length = 0;
	to->_maximum = 0;
	to->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (0);

	cc = GNOME_Evolution_Composer_RecipientList__alloc ();
	cc->_length = 0;
	cc->_maximum = 0;
	cc->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (0);

	bcc = GNOME_Evolution_Composer_RecipientList__alloc ();
	bcc->_length = 0;
	bcc->_maximum = 0;
	bcc->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (0);

	subject = CORBA_string_dup ("");

	CORBA_exception_init (&ev);
	GNOME_Evolution_Composer_setHeaders (composer, to, cc, 
					     bcc, subject, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		CORBA_free (to);
		CORBA_free (cc);
		CORBA_free (bcc);
		CORBA_free (subject);

		g_warning ("Error setting headers!");
		return;
	}

	CORBA_free (to);
	CORBA_free (cc);
	CORBA_free (bcc);
	CORBA_free (subject);

	GNOME_Evolution_Composer_show (composer, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		g_warning ("Error showing composer");
		return;
	}

	CORBA_exception_free (&ev);
	return;
}

static void
destroy_prefs_cb (GtkObject *object,
		  PropertyData *data)
{
	gtk_object_unref (data->xml);
	g_free (data);
}

static void
html_page_changed_cb (GtkEntry *entry,
		      PropertyData *data)
{
	ESummaryPrefs *prefs;

	/* Change the tmp prefs so that we can restore if the user cancels */
	prefs = data->esummary->tmp_prefs;

	if (prefs->page)
		g_free (prefs->page);

	prefs->page = g_strdup (gtk_entry_get_text (entry));

	gnome_property_box_changed (data->box);
}

static void
apply_prefs_cb (GnomePropertyBox *property_box,
		int page,
		ESummary *esummary)
{
	g_print ("Applying\n");

	if (page != -1)
		return;

	esummary->prefs = e_summary_prefs_copy (esummary->tmp_prefs);

	e_summary_reconfigure (esummary);
}

void
configure_summary (GtkWidget *widget,
		   ESummary *esummary)
{
	static GtkWidget *prefs = NULL;
	PropertyData *data;
	GtkWidget *html_page;

	if (prefs != NULL) {
		g_assert (GTK_WIDGET_REALIZED (prefs));
		gdk_window_show (prefs->window);
		gdk_window_raise (prefs->window);
		return;
	} 

	data = g_new (PropertyData, 1);
	data->esummary = esummary;
  
	if (esummary->tmp_prefs != NULL) {
		e_summary_prefs_free (esummary->tmp_prefs);
	}

	esummary->tmp_prefs = e_summary_prefs_copy (esummary->prefs);

	data->xml = glade_xml_new (EVOLUTION_GLADEDIR
				   "/executive-summary-config.glade", NULL);
	prefs = glade_xml_get_widget (data->xml, "summaryprefs");
	data->box = prefs;
	html_page = glade_xml_get_widget (data->xml, "htmlpage");

	if (esummary->prefs->page != NULL)
		gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (html_page))), esummary->prefs->page);

	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (html_page))),
			    "changed", GTK_SIGNAL_FUNC (html_page_changed_cb),
			    data);

	gtk_signal_connect (GTK_OBJECT (prefs), "apply",
			    GTK_SIGNAL_FUNC (apply_prefs_cb), esummary);

	gtk_signal_connect (GTK_OBJECT (prefs), "destroy",
			    GTK_SIGNAL_FUNC (destroy_prefs_cb), data);
	gtk_signal_connect (GTK_OBJECT (prefs), "destroy",
			    GTK_SIGNAL_FUNC (gtk_widget_destroyed), &prefs);
}

