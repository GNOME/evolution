/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Philip Van Hoof <pvanhoof@gnome.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlIO.h>
#include <libxml/xpath.h>

#include "format-handler.h"

static void	add_string_to_rdf		(xmlNodePtr node,
						 const gchar *tag,
						 const gchar *value);

/* Use { */

/* #include <calendar/gui/calendar-config-keys.h> */
/* #include <calendar/gui/calendar-config.h> */

/* } or { */
#define CALENDAR_CONFIG_PREFIX "/apps/evolution/calendar"
#define CALENDAR_CONFIG_TIMEZONE CALENDAR_CONFIG_PREFIX "/display/timezone"

static gchar *
calendar_config_get_timezone (void)
{
	GSettings *settings;
	gchar *retval = NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	retval = g_settings_get_string (settings, "timezone");
	g_object_unref (settings);
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
display_error_message (GtkWidget *parent,
                       const gchar *error_message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (
		GTK_WINDOW (parent), 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		"%s", error_message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/* Some helpers for the xml stuff */
static void
add_list_to_rdf (xmlNodePtr node,
                 const gchar *tag,
                 GSList *list_in,
                 gint type)
{
	if (list_in) {
		GSList *list = list_in;

		while (list) {
			const gchar *str = NULL;

			switch (type) {
			case ECALCOMPONENTATTENDEE:
				str = ((ECalComponentAttendee *) list->data)->value;
				break;
			case ECALCOMPONENTTEXT:
				str = ((ECalComponentText *) list->data)->value;
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
add_nummeric_to_rdf (xmlNodePtr node,
                     const gchar *tag,
                     gint *nummeric)
{
	if (nummeric) {
		gchar *value = g_strdup_printf ("%d", *nummeric);
		xmlNodePtr cur_node = xmlNewChild (node, NULL, (guchar *) tag, (guchar *) value);
		xmlSetProp (cur_node, (const guchar *)"rdf:datatype", (const guchar *)"http://www.w3.org/2001/XMLSchema#integer");
		g_free (value);
	}
}

static void
add_time_to_rdf (xmlNodePtr node,
                 const gchar *tag,
                 icaltimetype *time)
{
	if (time) {
		xmlNodePtr cur_node = NULL;
		struct tm mytm = icaltimetype_to_tm (time);
		gchar *str = (gchar *) g_malloc (sizeof (gchar) * 200);
		gchar *tmp = NULL;
		gchar *timezone;
		/*
		 * Translator: the %FT%T is the thirth argument for a strftime function.
		 * It lets you define the formatting of the date in the rdf-file.
		 * Also check out http://www.w3.org/2002/12/cal/tzd
		 * */
		e_utf8_strftime (str, 200, _("%FT%T"), &mytm);

		cur_node = xmlNewChild (node, NULL, (guchar *) tag, (guchar *) str);

		/* Not sure about this property */
		timezone = calendar_config_get_timezone ();
		tmp = g_strdup_printf ("http://www.w3.org/2002/12/cal/tzd/%s#tz", timezone);
		xmlSetProp (cur_node, (const guchar *)"rdf:datatype", (guchar *) tmp);
		g_free (tmp);
		g_free (timezone);
		g_free (str);
	}
}

static void
add_string_to_rdf (xmlNodePtr node,
                   const gchar *tag,
                   const gchar *value)
{
	if (value) {
		xmlNodePtr cur_node = NULL;
		cur_node = xmlNewChild (node, NULL, (guchar *) tag, (guchar *) value);
		xmlSetProp (cur_node, (const guchar *)"rdf:datatype", (const guchar *)"http://www.w3.org/2001/XMLSchema#string");
	}
}

static void
do_save_calendar_rdf (FormatHandler *handler,
                      ESourceSelector *selector,
		      EClientCache *client_cache,
                      gchar *dest_uri)
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
	EClient *source_client;
	GError *error = NULL;
	GSList *objects = NULL;
	gchar *temp = NULL;
	GOutputStream *stream;

	if (!dest_uri)
		return;

	/* open source client */
	primary_source = e_source_selector_ref_primary_selection (selector);
	source_client = e_client_cache_get_client_sync (client_cache,
		primary_source, e_source_selector_get_extension_name (selector), 30, NULL, &error);
	g_object_unref (primary_source);

	/* Sanity check. */
	g_return_if_fail (
		((source_client != NULL) && (error == NULL)) ||
		((source_client == NULL) && (error != NULL)));

	if (source_client == NULL) {
		display_error_message (
			gtk_widget_get_toplevel (GTK_WIDGET (selector)),
			error->message);
		g_error_free (error);
		return;
	}

	stream = open_for_writing (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (selector))), dest_uri, &error);

	if (stream && e_cal_client_get_object_list_as_comps_sync (E_CAL_CLIENT (source_client), "#t", &objects, NULL, NULL)) {
		GSList *iter;

		xmlBufferPtr buffer = xmlBufferCreate ();
		xmlDocPtr doc = xmlNewDoc ((xmlChar *) "1.0");
		xmlNodePtr fnode;

		doc->children = xmlNewDocNode (doc, NULL, (const guchar *)"rdf:RDF", NULL);
		xmlSetProp (doc->children, (const guchar *)"xmlns:rdf", (const guchar *)"http://www.w3.org/1999/02/22-rdf-syntax-ns#");
		xmlSetProp (doc->children, (const guchar *)"xmlns", (const guchar *)"http://www.w3.org/2002/12/cal/ical#");

		fnode = xmlNewChild (doc->children, NULL, (const guchar *)"Vcalendar", NULL);

		/* Should Evolution publicise these? */
		xmlSetProp (fnode, (const guchar *)"xmlns:x-wr", (const guchar *)"http://www.w3.org/2002/12/cal/prod/Apple_Comp_628d9d8459c556fa#");
		xmlSetProp (fnode, (const guchar *)"xmlns:x-lic", (const guchar *)"http://www.w3.org/2002/12/cal/prod/Apple_Comp_628d9d8459c556fa#");

		/* Not sure if it's correct like this */
		xmlNewChild (fnode, NULL, (const guchar *)"prodid", (const guchar *)"-//" PACKAGE_STRING "//iCal 1.0//EN");

		/* Assuming GREGORIAN is the only supported calendar scale */
		xmlNewChild (fnode, NULL, (const guchar *)"calscale", (const guchar *)"GREGORIAN");

		temp = calendar_config_get_timezone ();
		xmlNewChild (fnode, NULL, (const guchar *)"x-wr:timezone", (guchar *) temp);
		g_free (temp);

		xmlNewChild (fnode, NULL, (const guchar *)"method", (const guchar *)"PUBLISH");

		xmlNewChild (fnode, NULL, (const guchar *)"x-wr:relcalid", (guchar *) e_source_get_uid (primary_source));

		xmlNewChild (fnode, NULL, (const guchar *)"x-wr:calname", (guchar *) e_source_get_display_name (primary_source));

		/* Version of this RDF-format */
		xmlNewChild (fnode, NULL, (const guchar *)"version", (const guchar *)"2.0");

		for (iter = objects; iter; iter = iter->next) {
			ECalComponent *comp = iter->data;
			const gchar *temp_constchar;
			gchar *tmp_str = NULL;
			GSList *temp_list;
			ECalComponentDateTime temp_dt;
			struct icaltimetype *temp_time;
			gint *temp_int;
			ECalComponentText temp_comptext;
			xmlNodePtr c_node = xmlNewChild (fnode, NULL, (const guchar *)"component", NULL);
			xmlNodePtr node = xmlNewChild (c_node, NULL, (const guchar *)"Vevent", NULL);

			/* Getting the stuff */
			e_cal_component_get_uid (comp, &temp_constchar);
			tmp_str = g_strdup_printf ("#%s", temp_constchar);
			xmlSetProp (node, (const guchar *)"about", (guchar *) tmp_str);
			g_free (tmp_str);
			add_string_to_rdf (node, "uid",temp_constchar);

			e_cal_component_get_summary (comp, &temp_comptext);
			add_string_to_rdf (node, "summary", temp_comptext.value);

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
			e_cal_component_free_datetime (&temp_dt);

			e_cal_component_get_dtend (comp, &temp_dt);
			add_time_to_rdf (node, "dtend", temp_dt.value ? temp_dt.value : NULL);
			e_cal_component_free_datetime (&temp_dt);

			e_cal_component_get_due (comp, &temp_dt);
			add_time_to_rdf (node, "due", temp_dt.value ? temp_dt.value : NULL);
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
		}

		/* I used a buffer rather than xmlDocDump: I want gio support */
		xmlNodeDump (buffer, doc, doc->children, 2, 1);

		g_output_stream_write_all (stream, xmlBufferContent (buffer), xmlBufferLength (buffer), NULL, NULL, &error);
		g_output_stream_close (stream, NULL, NULL);

		e_cal_client_free_ecalcomp_slist (objects);

		xmlBufferFree (buffer);
		xmlFreeDoc (doc);
	}

	if (stream)
		g_object_unref (stream);

	g_object_unref (source_client);

	if (error != NULL) {
		display_error_message (
			gtk_widget_get_toplevel (GTK_WIDGET (selector)),
			error->message);
		g_error_free (error);
	}
}

FormatHandler *
rdf_format_handler_new (void)
{
	FormatHandler *handler = g_new0 (FormatHandler, 1);

	handler->isdefault = FALSE;
	handler->combo_label = _("RDF (.rdf)");
	handler->filename_ext = ".rdf";
	handler->options_widget = NULL;
	handler->save = do_save_calendar_rdf;

	return handler;
}
