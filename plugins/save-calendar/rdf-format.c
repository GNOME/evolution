/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Philip Van Hoof <pvanhoof@gnome.org>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <glib.h>
#include <glib/gi18n.h>
#ifdef USE_GTKFILECHOOSER
#  include <gtk/gtkfilechooser.h>
#  include <gtk/gtkfilechooserdialog.h>
#else
#  include <gtk/gtkfilesel.h>
#endif
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtk.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <libecal/e-cal.h>
#include <calendar/gui/e-cal-popup.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlIO.h>
#include <libxml/xpath.h>
#include <string.h>

#include "format-handler.h"

static void 
add_string_to_rdf (xmlNodePtr node, const gchar *tag, const char *value);

/* Use { */

/* #include <calendar/gui/calendar-config-keys.h> */
/* #include <calendar/gui/calendar-config.h> */

/* } or { */
#define CALENDAR_CONFIG_PREFIX "/apps/evolution/calendar"
#define CALENDAR_CONFIG_TIMEZONE CALENDAR_CONFIG_PREFIX "/display/timezone"

GConfClient *config = NULL;

static gchar *
calendar_config_get_timezone (void)
{
	gchar *retval = NULL;

	if (!config)
		config = gconf_client_get_default ();

	retval = gconf_client_get_string (config, CALENDAR_CONFIG_TIMEZONE, NULL);

	if (!retval) 
		retval = g_strdup ("UTC");

	return retval;
}
/* } */

enum { /* XML helper enum */
	ECALCOMPONENTTEXT,
	ECALCOMPONENTATTENDEE,
	CONSTCHAR
};

static void
display_error_message (GtkWidget *parent, GError *error)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent), 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
					 error->message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/* Some helpers for the xml stuff */
static void 
add_list_to_rdf (xmlNodePtr node, const gchar *tag, GSList *list_in, gint type)
{
	if (list_in) {
		GSList *list = list_in;

		while (list) {
			const char *str = NULL;

			switch (type) {
			case ECALCOMPONENTATTENDEE:
				str = ((ECalComponentAttendee*)list->data)->value;
				break;
			case ECALCOMPONENTTEXT:
				str = ((ECalComponentText*)list->data)->value;
				break;
			case CONSTCHAR:
			default:
				str = list->data;
				break;
			}

			add_string_to_rdf (node, tag, str);

			list = g_slist_next (list);
		}
	}
}

static void 
add_nummeric_to_rdf (xmlNodePtr node, const gchar *tag, gint *nummeric)
{
	if (nummeric) {
		gchar *value = g_strdup_printf ("%d", *nummeric);
		xmlNodePtr cur_node = xmlNewChild (node, NULL, tag, value);
		xmlSetProp (cur_node, "rdf:datatype", "http://www.w3.org/2001/XMLSchema#integer");
		g_free (value);
	}
}

static void
add_time_to_rdf (xmlNodePtr node, const gchar *tag, icaltimetype *time)
{
	if (time) {
		xmlNodePtr cur_node = NULL;
		gchar *tmp = NULL;
		gchar *str = g_strdup_printf ("%s%d-%s%d-%s%dT%s%d:%s%d:%s%d", 
					(time->year < 10)?"0":"", time->year, 
					(time->month < 10)?"0":"", time->month, 
					(time->day < 10)?"0":"", time->day, 
					(time->hour < 10)?"0":"", time->hour, 
					(time->minute < 10)?"0":"", time->minute, 
					(time->second < 10)?"0":"", time->second);
		cur_node = xmlNewChild (node, NULL, tag, str);

		/* Not sure about this property */
		tmp = g_strdup_printf ("http://www.w3.org/2002/12/cal/tzd/%s#tz", calendar_config_get_timezone ());
		xmlSetProp (cur_node, "rdf:datatype", tmp);
		g_free (tmp);

		g_free (str);
	}
}


static void 
add_string_to_rdf (xmlNodePtr node, const gchar *tag, const char *value)
{
	if (value) {
		xmlNodePtr cur_node = NULL;
		cur_node = xmlNewChild (node, NULL, tag, value);
		xmlSetProp (cur_node, "rdf:datatype", "http://www.w3.org/2001/XMLSchema#string");
	}
}




static void
do_save_calendar_rdf (FormatHandler *handler, EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type, char *dest_uri)
{

	/* 
	 * According to some documentation about CSV, newlines 'are' allowed 
	 * in CSV-files. But you 'do' have to put the value between quotes.
	 * The helper 'string_needsquotes' will check for that 
	 *
	 * http://www.creativyst.com/Doc/Articles/CSV/CSV01.htm
	 * http://www.creativyst.com/cgi-bin/Prod/15/eg/csv2xml.pl
	 */

	ESource *primary_source;
	ECal *source_client;
	GError *error = NULL;
	GList *objects=NULL;
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	GnomeVFSURI *uri;
	gchar *temp = NULL;
	gboolean doit = TRUE;

	if (!dest_uri)
		return;

	primary_source = e_source_selector_peek_primary_selection (target->selector);

	/* open source client */
	source_client = e_cal_new (primary_source, type);
	if (!e_cal_open (source_client, TRUE, &error)) {
		display_error_message (gtk_widget_get_toplevel (GTK_WIDGET (target->selector)), error);
		g_object_unref (source_client);
		g_error_free (error);
		return;
	}

	uri = gnome_vfs_uri_new (dest_uri);

	result = gnome_vfs_open_uri (&handle, uri, GNOME_VFS_OPEN_READ);
	if (result == GNOME_VFS_OK) {
		GtkWidget *warning = 
		  gtk_message_dialog_new_with_markup (NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_NONE,
				_("<b>File exists \"%s\".\n"
				  "Do you wish to overwrite it?"), dest_uri);

		gtk_dialog_add_button (GTK_DIALOG (warning), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (GTK_DIALOG (warning), _("_Overwrite"), GTK_RESPONSE_YES);

		doit = FALSE;
		if (gtk_dialog_run (GTK_DIALOG (warning)) == GTK_RESPONSE_YES)
			doit = TRUE;
		gtk_widget_destroy (warning);
	} 

	if (doit) {
		result = gnome_vfs_open_uri (&handle, uri, GNOME_VFS_OPEN_WRITE);
		if (result != GNOME_VFS_OK) {
			gnome_vfs_create (&handle, dest_uri, GNOME_VFS_OPEN_WRITE, TRUE, GNOME_VFS_PERM_USER_ALL);
			result = gnome_vfs_open_uri (&handle, uri, GNOME_VFS_OPEN_WRITE);
		} 
	}


	if (result == GNOME_VFS_OK && doit && e_cal_get_object_list_as_comp (source_client, "#t", &objects, NULL)) {
		xmlBufferPtr buffer=xmlBufferCreate();
		xmlDocPtr doc = xmlNewDoc((xmlChar *) "1.0");
		xmlNodePtr fnode = doc->children;

		doc->children = xmlNewDocNode (doc, NULL, "rdf:RDF", NULL);
		xmlSetProp (doc->children, "xmlns:rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
		xmlSetProp (doc->children, "xmlns", "http://www.w3.org/2002/12/cal/ical#");

		fnode = xmlNewChild (doc->children, NULL, "Vcalendar", NULL);

		/* Should Evolution publicise these? */
		xmlSetProp (fnode, "xmlns:x-wr", "http://www.w3.org/2002/12/cal/prod/Apple_Comp_628d9d8459c556fa#");
		xmlSetProp (fnode, "xmlns:x-lic", "http://www.w3.org/2002/12/cal/prod/Apple_Comp_628d9d8459c556fa#");

		/* Not sure if it's correct like this */
		xmlNewChild (fnode, NULL, "prodid", "-//" PACKAGE_STRING "//iCal 1.0//EN");

		/* Assuming GREGORIAN is the only supported calendar scale */
		xmlNewChild (fnode, NULL, "calscale", "GREGORIAN");

		temp = calendar_config_get_timezone ();
		xmlNewChild (fnode, NULL, "x-wr:timezone", temp);
		g_free (temp);

		xmlNewChild (fnode, NULL, "method", "PUBLISH");

		xmlNewChild (fnode, NULL, "x-wr:relcalid", e_source_peek_uid (primary_source));

		xmlNewChild (fnode, NULL, "x-wr:calname", e_source_peek_name (primary_source));

		/* Version of this RDF-format */
		xmlNewChild (fnode, NULL, "version", "2.0");

		while (objects != NULL) {
			ECalComponent *comp = objects->data;
			const char *temp_constchar;
			gchar *tmp_str = NULL;
			GSList *temp_list;
			ECalComponentDateTime temp_dt;
			struct icaltimetype *temp_time;
			int *temp_int;
			ECalComponentText temp_comptext; 
			xmlNodePtr c_node = xmlNewChild (fnode, NULL, "component", NULL);
			xmlNodePtr node = xmlNewChild (c_node, NULL, "Vevent", NULL);

			/* Getting the stuff */
			e_cal_component_get_uid (comp, &temp_constchar);
			tmp_str = g_strdup_printf ("#%s", temp_constchar);
			xmlSetProp (node, "about", tmp_str);
			g_free (tmp_str);
			add_string_to_rdf (node, "uid",temp_constchar);

			e_cal_component_get_summary (comp, &temp_comptext);
			add_string_to_rdf (node, "summary",&temp_comptext?temp_comptext.value:NULL);

			e_cal_component_get_description_list (comp, &temp_list);
			add_list_to_rdf (node, "description", temp_list, ECALCOMPONENTTEXT);
			if (temp_list)
				e_cal_component_free_text_list (temp_list);

			e_cal_component_get_categories_list (comp, &temp_list);
			add_list_to_rdf (node, "categories", temp_list, CONSTCHAR);
			if (temp_list)
				e_cal_component_free_categories_list (temp_list);

			e_cal_component_get_comment_list (comp, &temp_list);
			add_list_to_rdf (node, "comment", temp_list, ECALCOMPONENTTEXT);

			if (temp_list)
				e_cal_component_free_text_list (temp_list);

			e_cal_component_get_completed (comp, &temp_time);
			add_time_to_rdf (node, "completed", temp_time);
			if (temp_time)
				e_cal_component_free_icaltimetype (temp_time);

			e_cal_component_get_created (comp, &temp_time);
			add_time_to_rdf (node, "created", temp_time);
			if (temp_time)
				e_cal_component_free_icaltimetype (temp_time);

			e_cal_component_get_contact_list (comp, &temp_list);
			add_list_to_rdf (node, "contact", temp_list, ECALCOMPONENTTEXT);
			if (temp_list)
				e_cal_component_free_text_list (temp_list);

			e_cal_component_get_dtstart (comp, &temp_dt);
			 add_time_to_rdf (node, "dtstart", temp_dt.value ? temp_dt.value : NULL);
			if (temp_dt.value)
				e_cal_component_free_datetime (&temp_dt);

			e_cal_component_get_dtend (comp, &temp_dt);
			add_time_to_rdf (node, "dtend", temp_dt.value ? temp_dt.value : NULL);
			if (temp_dt.value)
				e_cal_component_free_datetime (&temp_dt);

			e_cal_component_get_due (comp, &temp_dt);
			add_time_to_rdf (node, "due", temp_dt.value ? temp_dt.value : NULL);
			if (temp_dt.value)
				e_cal_component_free_datetime (&temp_dt);

			e_cal_component_get_percent (comp, &temp_int);
			add_nummeric_to_rdf (node, "percentComplete", temp_int);

			e_cal_component_get_priority (comp, &temp_int);
			add_nummeric_to_rdf (node, "priority", temp_int);

			e_cal_component_get_url (comp, &temp_constchar);
			add_string_to_rdf (node, "URL", temp_constchar);

			if (e_cal_component_has_attendees (comp)) {
				e_cal_component_get_attendee_list (comp, &temp_list);
				add_list_to_rdf (node, "attendee", temp_list, ECALCOMPONENTATTENDEE);
				if (temp_list)
					e_cal_component_free_attendee_list (temp_list);
			}

			e_cal_component_get_location (comp, &temp_constchar);
			add_string_to_rdf (node, "location", temp_constchar);

			e_cal_component_get_last_modified (comp, &temp_time);
			add_time_to_rdf (node, "lastModified",temp_time);


			/* Important note!
			 * The documentation is not requiring this!
			 *
			 * if (temp_time) e_cal_component_free_icaltimetype (temp_time);
			 *
			 * Please uncomment and fix documentation if untrue
			 * http://www.gnome.org/projects/evolution/developer-doc/libecal/ECalComponent.html
			 *	#e-cal-component-get-last-modified
			 */

			objects = g_list_next (objects);
		}

		/* I used a buffer rather than xmlDocDump: I want gnome-vfs support */
		xmlNodeDump (buffer, doc, doc->children, 2, 1);

		gnome_vfs_write (handle, xmlBufferContent (buffer), xmlBufferLength (buffer), NULL);

		xmlBufferFree (buffer);
		xmlFreeDoc (doc);
		gnome_vfs_close (handle);
	}

	g_object_unref (source_client);

	return;
}

FormatHandler *rdf_format_handler_new (void)
{
	FormatHandler *handler = g_new (FormatHandler, 1);

	handler->isdefault = FALSE;
	handler->combo_label = _("RDF format (.rdf)");
	handler->filename_ext = ".rdf";
	handler->options_widget = NULL;
	handler->save = do_save_calendar_rdf;

	return handler;
}
