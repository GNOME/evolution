/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-itip-control.c
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
 *
 * Author: JP Rosevear
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib.h>
#include <gtk/gtkmisc.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-exception.h>
#include <gal/widgets/e-unicode.h>
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <ical.h>
#include <cal-util/cal-component.h>
#include <cal-util/timeutil.h>
#include <cal-client/cal-client.h>
#include <e-util/e-time-utils.h>
#include <e-util/e-dialog-widgets.h>
#include <evolution-shell-client.h>
#include <evolution-folder-selector-button.h>
#include <camel/camel-mime-filter-tohtml.h>
#include "calendar-config.h"
#include "itip-utils.h"
#include "e-itip-control.h"

struct _EItipControlPrivate {
	GtkWidget *html;
	gboolean html_destroyed;
	
	GPtrArray *event_clients;
	CalClient *event_client;
	GPtrArray *task_clients;
	CalClient *task_client;
	
	char *vcalendar;
	CalComponent *comp;
	icalcomponent *main_comp;
	icalcomponent *ical_comp;
	icalcomponent *top_level;
	icalcompiter iter;
	icalproperty_method method;

	int current;
	int total;

	gchar *calendar_uri;

	EAccountList *accounts;

	gchar *from_address;
	gchar *delegator_address;
	gchar *delegator_name;
	gchar *my_address;
	gboolean view_only;

	gboolean destroyed;
};

/* HTML Strings */
#define HTML_BODY_START "<body bgcolor=\"#ffffff\" text=\"#000000\" link=\"#336699\">"
#define HTML_SEP        "<hr color=#336699 align=\"left\" width=450>"
#define HTML_BODY_END   "</body>"
#define HTML_FOOTER     "</html>"

extern EvolutionShellClient *global_shell_client;	

/* We intentionally use "calendar" instead of "calendar / *" here. We
 * don't want public calendars.
 */
static const char *calendar_types[] = { "calendar", NULL };
static const char *tasks_types[] = { "tasks", NULL };

static void class_init	(EItipControlClass	 *klass);
static void init	(EItipControl		 *itip);
static void destroy	(GtkObject               *obj);
static void finalize	(GObject               *obj);

static void url_requested_cb (GtkHTML *html, const gchar *url, GtkHTMLStream *handle, gpointer data);
static gboolean object_requested_cb (GtkHTML *html, GtkHTMLEmbedded *eb, gpointer data);
static void ok_clicked_cb (GtkHTML *html, const gchar *method, const gchar *url, const gchar *encoding, gpointer data);

static GtkVBoxClass *parent_class = NULL;

E_MAKE_TYPE (e_itip_control, "EItipControl", EItipControl, class_init, init,
	     GTK_TYPE_VBOX);

static void
class_init (EItipControlClass *klass)
{
	GObjectClass *object_class;
	GtkObjectClass *gtkobject_class;
	
	object_class = G_OBJECT_CLASS (klass);
	gtkobject_class = GTK_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);

	gtkobject_class->destroy = destroy;

	object_class->finalize = finalize;
}


/* Calendar Server routines */
static void
start_calendar_server_cb (CalClient *cal_client,
			  CalClientOpenStatus status,
			  gpointer data)
{
	gboolean *success = data;

	if (status == CAL_CLIENT_OPEN_SUCCESS)
		*success = TRUE;
	else
		*success = FALSE;

	gtk_main_quit (); /* end the sub event loop */
}

static CalClient *
start_calendar_server (EItipControl *itip, char *uri)
{
	CalClient *client;
	gboolean success = FALSE;

	client = cal_client_new ();

	g_signal_connect (client, "cal_opened", G_CALLBACK (start_calendar_server_cb), &success);

 	if (!cal_client_open_calendar (client, uri, TRUE))
 		goto error;
	
	/* run a sub event loop to turn cal-client's async load
	   notification into a synchronous call */
 	if (!itip->priv->destroyed) {
 		gtk_signal_connect (GTK_OBJECT (itip), "destroy",
 				    gtk_main_quit, NULL);
	
		gtk_main ();
	
 		gtk_signal_disconnect_by_func (GTK_OBJECT (itip),
 					       gtk_main_quit, NULL);
 	}

	if (success)
		return client;

error:
	g_object_unref (client);
	
	return NULL;
}

static CalClient *
start_default_server (EItipControl *itip, gboolean tasks)
{
	CalClient *client;
	gboolean success = FALSE;

	client = cal_client_new ();

	g_signal_connect (client, "cal_opened", G_CALLBACK (start_calendar_server_cb), &success);

	if (tasks) {
		if (!cal_client_open_default_tasks (client, FALSE))
			goto error;
	} else {
		if (!cal_client_open_default_calendar (client, FALSE))
			goto error;
	}
	
	/* run a sub event loop to turn cal-client's async load
	   notification into a synchronous call */
	if (!itip->priv->destroyed) {
		gtk_signal_connect (GTK_OBJECT (itip), "destroy",
				    gtk_main_quit, NULL);

		gtk_main ();
		
		gtk_signal_disconnect_by_func (GTK_OBJECT (itip),
					       gtk_main_quit, NULL);
	}
	
	if (success)
		return client;

 error:
	g_object_unref (client);
	
	return NULL;
}

static GPtrArray *
get_servers (EItipControl *itip, EvolutionShellClient *shell_client, const char *possible_types[], gboolean tasks)
{
	GNOME_Evolution_StorageRegistry registry;
	GNOME_Evolution_StorageRegistry_StorageList *storage_list;
	GPtrArray *servers;
	int i, j, k;
	CORBA_Environment ev;
	
	servers = g_ptr_array_new ();
	
	g_object_ref (shell_client);
	registry = evolution_shell_client_get_storage_registry_interface (shell_client);
	
	CORBA_exception_init (&ev);
	storage_list = GNOME_Evolution_StorageRegistry_getStorageList (registry, &ev);
	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		return servers;
	}
	
	for (i = 0; i < storage_list->_length; i++) {
		GNOME_Evolution_Storage storage;
		GNOME_Evolution_FolderList *folder_list;

		CORBA_exception_init (&ev);
		
		storage = storage_list->_buffer[i];
		folder_list = GNOME_Evolution_Storage__get_folderList (storage, &ev);
		if (BONOBO_EX (&ev)) {
			CORBA_exception_free (&ev);
			continue;
		}

		CORBA_exception_free (&ev);

		for (j = 0; j < folder_list->_length; j++) {
			GNOME_Evolution_Folder folder;
			
			folder = folder_list->_buffer[j];
			for (k = 0; possible_types[k] != NULL; k++) {
				CalClient *client;
				char *uri;
				
				if (itip->priv->destroyed)
					continue;
				
				if (strcmp (possible_types[k], folder.type))
					continue;

				uri = cal_util_expand_uri (folder.physicalUri, tasks);
				client = start_calendar_server (itip, uri);
				if (client != NULL)
					g_ptr_array_add (servers, client);			
				g_free (uri);

				break;
			}
		}

		CORBA_free (folder_list);		
	}
	
	g_object_unref (shell_client);

	return servers;
}

static CalClient *
find_server (GPtrArray *servers, CalComponent *comp)
{
	const char *uid;
	int i;

	cal_component_get_uid (comp, &uid);	
	for (i = 0; i < servers->len; i++) {
		CalClient *client;
		CalComponent *found_comp;
		CalClientGetStatus status;
		
		client = g_ptr_array_index (servers, i);
		status = cal_client_get_object (client, uid, &found_comp);
		if (status == CAL_CLIENT_GET_SUCCESS) {
			g_object_unref (found_comp);
			g_object_ref (client);

			return client;
		}
	}

	return NULL;
}

static void
html_destroyed (gpointer data)
{
	EItipControl *itip = data;
	EItipControlPrivate *priv;
	
	priv = itip->priv;
	
	priv->html_destroyed = TRUE;
}

static void
init (EItipControl *itip)
{
	EItipControlPrivate *priv;
	GtkWidget *scrolled_window;

	priv = g_new0 (EItipControlPrivate, 1);

	itip->priv = priv;

	/* Addresses */
	priv->accounts = itip_addresses_get ();

	/* Initialize the cal clients */
	priv->event_clients = NULL;
	priv->event_client = NULL;
	priv->task_clients = NULL;
	priv->task_client = NULL;

	/* Other fields to init */
	priv->calendar_uri = NULL;
	priv->from_address = NULL;
	priv->delegator_address = NULL;
	priv->delegator_name = NULL;
	priv->my_address = NULL;
	priv->view_only = FALSE;
	
	/* Html Widget */
	priv->html = gtk_html_new ();
	priv->html_destroyed = FALSE;
	gtk_html_set_default_content_type (GTK_HTML (priv->html), 
					   "text/html; charset=utf-8");
	gtk_html_load_from_string (GTK_HTML (priv->html), " ", 1);
	gtk_widget_show (priv->html);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scrolled_window);

	gtk_container_add (GTK_CONTAINER (scrolled_window), priv->html);
	gtk_object_weakref (GTK_OBJECT (priv->html), html_destroyed, itip);
	gtk_widget_set_usize (scrolled_window, 600, 400);
	gtk_box_pack_start (GTK_BOX (itip), scrolled_window, FALSE, FALSE, 4);

	g_signal_connect (priv->html, "url_requested", G_CALLBACK (url_requested_cb), itip);
	g_signal_connect (priv->html, "object_requested", G_CALLBACK (object_requested_cb), itip);
	g_signal_connect (priv->html, "submit", G_CALLBACK (ok_clicked_cb), itip);

	priv->destroyed = FALSE;
}

static void
clean_up (EItipControl *itip)
{
	EItipControlPrivate *priv;

	priv = itip->priv;
	if (!priv)
		return;

	g_free (priv->vcalendar);
	priv->vcalendar = NULL;

	if (priv->comp) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	if (priv->top_level) {
		icalcomponent_free (priv->top_level);
		priv->top_level = NULL;
	}

	if (priv->main_comp) {
		icalcomponent_free (priv->main_comp);
		priv->main_comp = NULL;
	}
	priv->ical_comp = NULL;

	priv->current = 0;
	priv->total = 0;

	g_free (priv->calendar_uri);
	priv->calendar_uri = NULL;

	g_free (priv->from_address);
	priv->from_address = NULL;
	g_free (priv->delegator_address);
	priv->delegator_address = NULL;
	g_free (priv->delegator_name);
	priv->delegator_name = NULL;
	g_free (priv->my_address);
	priv->my_address = NULL;
}

static void
destroy (GtkObject *obj)
{
	EItipControl *itip = E_ITIP_CONTROL (obj);
	EItipControlPrivate *priv;
	
	priv = itip->priv;
	  
	priv->destroyed = TRUE;
}

static void
finalize (GObject *obj)
{
	EItipControl *itip = E_ITIP_CONTROL (obj);
	EItipControlPrivate *priv;
	int i;
	
	priv = itip->priv;

	clean_up (itip);

 	if (priv->html)
 		gtk_object_weakunref (GTK_OBJECT (priv->html), html_destroyed, itip);

	priv->accounts = NULL;
	
	if (priv->event_clients) {
		for (i = 0; i < priv->event_clients->len; i++) 
			g_object_unref (g_ptr_array_index (priv->event_clients, i));
		g_ptr_array_free (priv->event_clients, TRUE);
		priv->event_client = NULL;
		priv->event_clients = NULL;
	}
	
	if (priv->task_clients) {
		for (i = 0; i < priv->task_clients->len; i++) 
			g_object_unref (g_ptr_array_index (priv->task_clients, i));
		g_ptr_array_free (priv->task_clients, TRUE);
		priv->task_client = NULL;
		priv->task_clients = NULL;
	}
	
	g_free (priv);
	itip->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (obj);
}

GtkWidget *
e_itip_control_new (void)
{
	return g_object_new (E_TYPE_ITIP_CONTROL, NULL);
}

static void
find_my_address (EItipControl *itip, icalcomponent *ical_comp)
{
	EItipControlPrivate *priv;
	icalproperty *prop;
	char *my_alt_address = NULL;
	
	priv = itip->priv;

	/* If the mailer told us the address to use, use that */
	if (priv->delegator_address != NULL) {
		priv->my_address = g_strdup (itip_strip_mailto (priv->delegator_address));
		priv->my_address = g_strstrip (priv->my_address);
		return;
	}
	
	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY))
	{
		icalvalue *value;
		icalparameter *param;
		const char *attendee, *name;
		char *attendee_clean, *name_clean;
		EIterator *it;

		value = icalproperty_get_value (prop);
		if (value != NULL) {
			attendee = icalvalue_get_string (value);
			attendee_clean = g_strdup (itip_strip_mailto (attendee));
			attendee_clean = g_strstrip (attendee_clean);
		} else {
			attendee = NULL;
			attendee_clean = NULL;
		}
		
		param = icalproperty_get_first_parameter (prop, ICAL_CN_PARAMETER);
		if (param != NULL) {
			name = icalparameter_get_cn (param);
			name_clean = g_strdup (name);
			name_clean = g_strstrip (name_clean);
		} else {
			name = NULL;
			name_clean = NULL;
		}

		it = e_list_get_iterator((EList *)priv->accounts);
		while (e_iterator_is_valid(it)) {
			const EAccount *account = e_iterator_get(it);

			/* Check for a matching address */
			if (attendee_clean != NULL
			    && !g_ascii_strcasecmp (account->id->address, attendee_clean)) {
				priv->my_address = g_strdup (account->id->address);
				g_free (attendee_clean);
				g_free (name_clean);
				g_free (my_alt_address);
				g_object_unref(it);
				return;
			}
			
			/* Check for a matching cname to fall back on */
			if (name_clean != NULL 
			    && !g_ascii_strcasecmp (account->id->name, name_clean))
				my_alt_address = g_strdup (attendee_clean);

			e_iterator_next(it);
		}
		g_free (attendee_clean);
		g_free (name_clean);
		g_object_unref(it);
	}

	priv->my_address = my_alt_address;
}

static icalproperty *
find_attendee (icalcomponent *ical_comp, const char *address)
{
	icalproperty *prop;

	if (address == NULL)
		return NULL;

	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY))
	{
		icalvalue *value;
		const char *attendee;
		char *text;

		value = icalproperty_get_value (prop);
		if (!value)
			continue;

		attendee = icalvalue_get_string (value);

		text = g_strdup (itip_strip_mailto (attendee));
		text = g_strstrip (text);
		if (!g_strcasecmp (address, text)) {
			g_free (text);
			break;
		}
		g_free (text);
	}
	
	return prop;
}

static void
write_label_piece (EItipControl *itip, CalComponentDateTime *dt,
		   char *buffer, int size,
		   const char *stext, const char *etext,
		   gboolean just_date)
{
	EItipControlPrivate *priv;
	struct tm tmp_tm;
	char time_buf[64], *time_utf8;
	icaltimezone *zone = NULL;
	char *display_name;

	priv = itip->priv;

	/* UTC times get converted to the current timezone. This is done for
	   the COMPLETED property, which is always in UTC, and also because
	   Outlook sends simple events as UTC times. */
	if (dt->value->is_utc) {
		char *location = calendar_config_get_timezone ();
		zone = icaltimezone_get_builtin_timezone (location);
		icaltimezone_convert_time (dt->value, icaltimezone_get_utc_timezone (), zone);
	}

	tmp_tm = icaltimetype_to_tm (dt->value);
	if (just_date)
		tmp_tm.tm_hour = tmp_tm.tm_min = tmp_tm.tm_sec = 0;

	if (stext != NULL)
		strcat (buffer, stext);

	e_time_format_date_and_time (&tmp_tm,
				     calendar_config_get_24_hour_format (),
				     FALSE, FALSE,
				     time_buf, sizeof (time_buf));

	time_utf8 = g_locale_to_utf8 (time_buf, -1, NULL, NULL, NULL);
	strcat (buffer, time_utf8);
	g_free (time_utf8);

	if (!dt->value->is_utc && dt->tzid) {
		zone = icalcomponent_get_timezone (priv->top_level, dt->tzid);
	}

	/* Output timezone after time, e.g. " America/New_York". */
	if (zone && !just_date) {
		/* Note that this returns UTF-8, since all iCalendar data is
		   UTF-8. But it probably is not translated. */
		display_name = icaltimezone_get_display_name (zone);
		if (display_name && *display_name) {
			strcat (buffer, " <font size=-1>[");

			/* We check if it is one of our builtin timezone names,
			   in which case we call gettext to translate it. */
			if (icaltimezone_get_builtin_timezone (display_name)) {
				strcat (buffer, _(display_name));
			} else {
				strcat (buffer, display_name);
			}
			strcat (buffer, "]</font>");
		}
	}

	if (etext != NULL)
		strcat (buffer, etext);
}

static const char *
nth (int n)
{
	if (n == -1)
		return "last";
	else if (n < 1 || n > 31)
		return "?";
	else
		return cal_recur_nth[n];
}

static const char *dayname[] = {
	N_("Sunday"),
	N_("Monday"),
	N_("Tuesday"),
	N_("Wednesday"),
	N_("Thursday"),
	N_("Friday"),
	N_("Saturday")
};

static inline char *
get_dayname (struct icalrecurrencetype *r, int i)
{
	enum icalrecurrencetype_weekday day;

	day = icalrecurrencetype_day_day_of_week (r->by_day[i]);
	g_return_val_if_fail (day > 0 && day < 8, "?");

	return _(dayname[day - 1]);
}

static void
write_recurrence_piece (EItipControl *itip, CalComponent *comp,
			char *buffer, int size)
{
	GSList *rrules;
	struct icalrecurrencetype *r;
	int len, i;

	strcpy (buffer, "<b>Recurring:</b> ");
	len = strlen (buffer);
	buffer += len;
	size -= len;

	if (!cal_component_has_simple_recurrence (comp)) {
		strcpy (buffer, _("Yes. (Complex Recurrence)"));
		return;
	}

	cal_component_get_rrule_list (comp, &rrules);
	g_return_if_fail (rrules && !rrules->next);

	r = rrules->data;

	switch (r->freq) {
	case ICAL_DAILY_RECURRENCE:
		if (r->interval == 1)
			strcpy (buffer, _("Every day"));
		else
			sprintf (buffer, _("Every %d days"), r->interval);
		break;

	case ICAL_WEEKLY_RECURRENCE:
		if (r->by_day[0] == ICAL_RECURRENCE_ARRAY_MAX) {
			if (r->interval == 1)
				strcpy (buffer, _("Every week"));
			else
				sprintf (buffer, _("Every %d weeks"), r->interval);
		} else {
			if (r->interval == 1)
				strcpy (buffer, _("Every week on "));
			else
				sprintf (buffer, _("Every %d weeks on "), r->interval);

			for (i = 1; i < 8 && r->by_day[i] != ICAL_RECURRENCE_ARRAY_MAX; i++) {
				if (i > 1)
					strcat (buffer, ", ");
				strcat (buffer, get_dayname (r, i - 1));
			}
			if (i > 1)
				strcat (buffer, _(" and "));
			strcat (buffer, get_dayname (r, i - 1));
		}
		break;

	case ICAL_MONTHLY_RECURRENCE:
		if (r->by_month_day[0] != ICAL_RECURRENCE_ARRAY_MAX) {
			sprintf (buffer, _("The %s day of "),
				 nth (r->by_month_day[0]));
		} else {
			int pos;

			/* Outlook 2000 uses BYDAY=TU;BYSETPOS=2, and will not
			   accept BYDAY=2TU. So we now use the same as Outlook
			   by default. */

			pos = icalrecurrencetype_day_position (r->by_day[0]);
			if (pos == 0)
				pos = r->by_set_pos[0];

			sprintf (buffer, _("The %s %s of "),
				 nth (pos), get_dayname (r, 0));
		}

		if (r->interval == 1)
			strcat (buffer, _("every month"));
		else {
			len = strlen (buffer);
			buffer += len;
			size -= len;
			sprintf (buffer, _("every %d months"), r->interval);
		}
		break;

	case ICAL_YEARLY_RECURRENCE:
		if (r->interval == 1)
			strcpy (buffer, _("Every year"));
		else {
			sprintf (buffer, _("Every %d years"), r->interval);
		}
		break;

	default:
		g_assert_not_reached ();
	}

	len = strlen (buffer);
	buffer += len;
	size -= len;
	if (r->count) {
		sprintf (buffer, _(" a total of %d times"), r->count);
	} else if (!icaltime_is_null_time (r->until)) {
		CalComponentDateTime dt;

		dt.value = &r->until;
		dt.tzid = r->until.zone;

		write_label_piece (itip, &dt, buffer, size,
				   _(", ending on "), NULL, TRUE);
	}

	strcat (buffer, "<br>");
}

static void
set_date_label (EItipControl *itip, GtkHTML *html, GtkHTMLStream *html_stream,
		CalComponent *comp)
{
	EItipControlPrivate *priv;
	CalComponentDateTime datetime;
	static char buffer[1024];
	gboolean wrote = FALSE, task_completed = FALSE;
	CalComponentVType type;

	priv = itip->priv;

	type = cal_component_get_vtype (comp);

	buffer[0] = '\0';
	cal_component_get_dtstart (comp, &datetime);
	if (datetime.value) {
		write_label_piece (itip, &datetime, buffer, 1024,
				   _("<b>Starts:</b> "),
				   "<br>", FALSE);
		gtk_html_write (html, html_stream, buffer, strlen(buffer));
		wrote = TRUE;
	}
	cal_component_free_datetime (&datetime);

	buffer[0] = '\0';
	cal_component_get_dtend (comp, &datetime);
	if (datetime.value){
		write_label_piece (itip, &datetime, buffer, 1024, _("<b>Ends:</b> "), "<br>", FALSE);
		gtk_html_write (html, html_stream, buffer, strlen (buffer));
		wrote = TRUE;
	}
	cal_component_free_datetime (&datetime);

	buffer[0] = '\0';
	if (cal_component_has_recurrences (comp)) {
		write_recurrence_piece (itip, comp, buffer, 1024);
		gtk_html_write (html, html_stream, buffer, strlen (buffer));
		wrote = TRUE;
	}

	buffer[0] = '\0';
	datetime.tzid = NULL;
	cal_component_get_completed (comp, &datetime.value);
	if (type == CAL_COMPONENT_TODO && datetime.value) {
		/* Pass TRUE as is_utc, so it gets converted to the current
		   timezone. */
		datetime.value->is_utc = TRUE;
		write_label_piece (itip, &datetime, buffer, 1024, _("<b>Completed:</b> "), "<br>", FALSE);
		gtk_html_write (html, html_stream, buffer, strlen (buffer));
		wrote = TRUE;
		task_completed = TRUE;
	}
	cal_component_free_datetime (&datetime);

	buffer[0] = '\0';
	cal_component_get_due (comp, &datetime);
	if (type == CAL_COMPONENT_TODO && !task_completed && datetime.value) {
		write_label_piece (itip, &datetime, buffer, 1024, _("<b>Due:</b> "), "<br>", FALSE);
		gtk_html_write (html, html_stream, buffer, strlen (buffer));
		wrote = TRUE;
	}

	cal_component_free_datetime (&datetime);

	if (wrote)
		gtk_html_stream_printf (html_stream, "<br>");
}

static void
set_message (GtkHTML *html, GtkHTMLStream *html_stream, const gchar *message, gboolean err)
{
	if (message == NULL)
		return;


	if (err) {
		gtk_html_stream_printf (html_stream, "<b><font color=\"#ff0000\">%s</font></b><br><br>", message);
	} else {
		gtk_html_stream_printf (html_stream, "<b>%s</b><br><br>", message);
	}
}

static void
write_error_html (EItipControl *itip, const gchar *itip_err)
{
	EItipControlPrivate *priv;
	GtkHTMLStream *html_stream;

	priv = itip->priv;

	/* Html widget */
	html_stream = gtk_html_begin (GTK_HTML (priv->html));
	gtk_html_stream_printf (html_stream,
				"<html><head><title>%s</title></head>",
				_("iCalendar Information"));

	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_BODY_START, strlen(HTML_BODY_START));

	/* The table */
	gtk_html_stream_printf (html_stream, "<table width=450 cellspacing=\"0\" cellpadding=\"4\" border=\"0\">");
	/* The column for the image */
	gtk_html_stream_printf (html_stream, "<tr><td width=48 align=\"center\" valign=\"top\" rowspan=\"8\">");
	/* The image */
	gtk_html_stream_printf (html_stream, "<img src=\"/meeting-request.png\"></td>");

	gtk_html_stream_printf (html_stream, "<td align=\"left\" valign=\"top\">");

	/* Title */
	set_message (GTK_HTML (priv->html), html_stream, _("iCalendar Error"), TRUE);

	/* Error */
	gtk_html_write (GTK_HTML (priv->html), html_stream, itip_err, strlen(itip_err));

	/* Clean up */
	gtk_html_stream_printf (html_stream, "</td></tr></table>");

	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_BODY_END, strlen(HTML_BODY_END));
	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_FOOTER, strlen(HTML_FOOTER));

	gtk_html_end (GTK_HTML (priv->html), html_stream, GTK_HTML_STREAM_OK);
}

static void
write_html (EItipControl *itip, const gchar *itip_desc, const gchar *itip_title, const gchar *options)
{
	EItipControlPrivate *priv;
	GtkHTMLStream *html_stream;
	CalComponentText text;
	CalComponentOrganizer organizer;
	CalComponentAttendee *attendee;
	GSList *attendees, *l = NULL;
	const char *string;
	gchar *html;
	const gchar *const_html;

	priv = itip->priv;

	if (priv->html_destroyed)
		return;
	
	/* Html widget */
	html_stream = gtk_html_begin (GTK_HTML (priv->html));
	gtk_html_stream_printf (html_stream,
				"<html><head><title>%s</title></head>",
				_("iCalendar Information"));
	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_BODY_START, strlen(HTML_BODY_START));

	/* The table */
	const_html = "<table width=450 cellspacing=\"0\" cellpadding=\"4\" border=\"0\">";
	gtk_html_write (GTK_HTML (priv->html), html_stream, const_html, strlen(const_html));

	/* The column for the image */
	const_html = "<tr><td width=48 align=\"center\" valign=\"top\" rowspan=\"8\">";
	gtk_html_write (GTK_HTML (priv->html), html_stream, const_html, strlen(const_html));

	/* The image */
	const_html = "<img src=\"/meeting-request.png\"></td>";
	gtk_html_write (GTK_HTML (priv->html), html_stream, const_html, strlen(const_html));

	const_html = "<td align=\"left\" valign=\"top\">";
	gtk_html_write (GTK_HTML (priv->html), html_stream, const_html, strlen(const_html));

	switch (priv->method) {
	case ICAL_METHOD_REFRESH:
	case ICAL_METHOD_REPLY:
		/* An attendee sent this */
		cal_component_get_attendee_list (priv->comp, &attendees);
		if (attendees != NULL) {
			attendee = attendees->data;
			html = g_strdup_printf (itip_desc,
						attendee->cn ?
						attendee->cn :
						itip_strip_mailto (attendee->value));
		} else {
			html = g_strdup_printf (itip_desc, _("An unknown person"));
		}
		break;
	case ICAL_METHOD_REQUEST:
		/* The organizer sent this */
		cal_component_get_organizer (priv->comp, &organizer);
		if (priv->delegator_address != NULL) {
			if (organizer.value != NULL)
				html = g_strdup_printf (itip_desc,
							organizer.cn ?
							organizer.cn :
							itip_strip_mailto (organizer.value),
							priv->delegator_name ?
							priv->delegator_name :
					                priv->delegator_address);
			else
				html = g_strdup_printf (itip_desc, _("An unknown person"),
							priv->delegator_name ?
							priv->delegator_name :
					                priv->delegator_address);
		} else {
			if (organizer.value != NULL)
				html = g_strdup_printf (itip_desc,
							organizer.cn ?
							organizer.cn :
							itip_strip_mailto (organizer.value));
			else
				html = g_strdup_printf (itip_desc, _("An unknown person"));
		}
		
		break;
		
	case ICAL_METHOD_PUBLISH:
	case ICAL_METHOD_ADD:
	case ICAL_METHOD_CANCEL:
	default:
		/* The organizer sent this */
		cal_component_get_organizer (priv->comp, &organizer);
		if (organizer.value != NULL)
			html = g_strdup_printf (itip_desc,
						organizer.cn ?
						organizer.cn :
						itip_strip_mailto (organizer.value));
		else
			html = g_strdup_printf (itip_desc, _("An unknown person"));
		break;
	}
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	/* Describe what the user can do */
	const_html = _("<br> Please review the following information, "
			"and then select an action from the menu below.");
	gtk_html_write (GTK_HTML (priv->html), html_stream, const_html, strlen(const_html));

	/* Separator */
	gtk_html_write (GTK_HTML (priv->html), html_stream, HTML_SEP, strlen (HTML_SEP));

	/* Title */
	set_message (GTK_HTML (priv->html), html_stream, itip_title, FALSE);

	/* Date information */
	set_date_label (itip, GTK_HTML (priv->html), html_stream, priv->comp);

	/* Summary */
	cal_component_get_summary (priv->comp, &text);
	html = text.value ? camel_text_to_html (text.value, CAMEL_MIME_FILTER_TOHTML_CONVERT_NL, 0) : _("<i>None</i>");
	gtk_html_stream_printf (html_stream, "<b>%s</b><br>%s<br><br>",
				_("Summary:"), html);
	if (text.value)
		g_free (html);
	
	/* Location */
	cal_component_get_location (priv->comp, &string);
	if (string != NULL) {
		html = camel_text_to_html (string, CAMEL_MIME_FILTER_TOHTML_CONVERT_NL, 0);
		gtk_html_stream_printf (html_stream, "<b>%s</b><br>%s<br><br>", 
					_("Location:"), html);
		g_free (html);
	}
	
	/* Status */
	if (priv->method == ICAL_METHOD_REPLY) {
		GSList *alist;

		cal_component_get_attendee_list (priv->comp, &alist);
		
		if (alist != NULL) {
			CalComponentAttendee *a = alist->data;

			gtk_html_stream_printf (html_stream, "<b>%s</b><br>",
						_("Status:"));
			
			switch (a->status) {
			case ICAL_PARTSTAT_ACCEPTED:
				gtk_html_stream_printf (html_stream, "%s<br><br>",
							_("Accepted"));
				break;
			case ICAL_PARTSTAT_TENTATIVE:
				gtk_html_stream_printf (html_stream, "%s<br><br>",
							_("Tentatively Accepted"));
				break;
			case ICAL_PARTSTAT_DECLINED:
				gtk_html_stream_printf (html_stream, "%s<br><br>",
							_("Declined"));
				break;
			default:
				gtk_html_stream_printf (html_stream, "%s<br><br>",
							_("Unknown"));
			}
		}
		
		cal_component_free_attendee_list (alist);
	}
	
	/* Description */
	cal_component_get_description_list (priv->comp, &l);
	if (l)
		text = *((CalComponentText *)l->data);

	if (l && text.value) {
		html = camel_text_to_html (text.value, CAMEL_MIME_FILTER_TOHTML_CONVERT_NL, 0);
		gtk_html_stream_printf (html_stream, "<b>%s</b><br>%s",
					_("Description:"), html);
		g_free (html);
	}
	cal_component_free_text_list (l);

	/* Separator */
	gtk_html_write (GTK_HTML (priv->html), html_stream, HTML_SEP, strlen (HTML_SEP));

	/* Options */
	if (!e_itip_control_get_view_only (itip)) {
		if (options != NULL) {
			const_html = "</td></tr><tr><td valign=\"center\">";
			gtk_html_write (GTK_HTML (priv->html), html_stream, const_html, strlen (const_html));
			gtk_html_write (GTK_HTML (priv->html), html_stream, options, strlen (options));
		}
	}
	
	const_html = "</td></tr></table>";
	gtk_html_write (GTK_HTML (priv->html), html_stream, const_html, strlen(const_html));

	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_BODY_END, strlen(HTML_BODY_END));
	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_FOOTER, strlen(HTML_FOOTER));

	gtk_html_end (GTK_HTML (priv->html), html_stream, GTK_HTML_STREAM_OK);
}


static char*
get_publish_options (gboolean selector)
{
	char *html;
	
	html = g_strdup_printf ("<form><b>%s</b>&nbsp"
				"<select NAME=\"action\" SIZE=\"1\"> "
				"<option VALUE=\"U\">%s</option>"
				"</select>&nbsp &nbsp "
				"<input TYPE=Submit name=\"ok\" value=\"%s\">"
				"</form>",
				_("Choose an action:"),
				_("Update"),
				_("OK"));

	if (selector) {
		char *sel;
		
		sel = g_strconcat (html, "<object classid=\"gtk:label\">", NULL);
		g_free (html);
		html = sel;
	}
	
	return html;
}

static char*
get_request_options (gboolean selector)
{
	char *html;
	
	html = g_strdup_printf ("<form><b>%s</b>&nbsp"
				"<select NAME=\"action\" SIZE=\"1\"> "
				"<option VALUE=\"A\">%s</option> "
				"<option VALUE=\"T\">%s</option> "
				"<option VALUE=\"D\">%s</option></select>&nbsp "
				"<input TYPE=\"checkbox\" name=\"rsvp\" value=\"1\" checked>%s&nbsp&nbsp"
				"<input TYPE=\"submit\" name=\"ok\" value=\"%s\"><br> "
				"</form>",
				_("Choose an action:"),
				_("Accept"),
				_("Tentatively accept"),
				_("Decline"),
				_("RSVP"),
				_("OK"));

	if (selector) {
		char *sel;
		
		sel = g_strconcat (html, "<object classid=\"gtk:label\">", NULL);
		g_free (html);
		html = sel;
	}
	
	return html;
}

static char*
get_request_fb_options ()
{
	return g_strdup_printf ("<form><b>%s</b>&nbsp"
				"<select NAME=\"action\" SIZE=\"1\"> "
				"<option VALUE=\"F\">%s</option></select>&nbsp &nbsp "
				"<input TYPE=Submit name=\"ok\" value=\"%s\">"
				"</form>",
				_("Choose an action:"),
				_("Send Free/Busy Information"),
				_("OK"));
}

static char*
get_reply_options ()
{
	return g_strdup_printf ("<form><b>%s</b>&nbsp"
				"<select NAME=\"action\" SIZE=\"1\"> "
				"<option VALUE=\"R\">%s</option></select>&nbsp &nbsp "
				"<input TYPE=Submit name=\"ok\" value=\"%s\">"
				"</form>",
				_("Choose an action:"),
				_("Update respondent status"),
				_("OK"));
}

static char*
get_refresh_options ()
{
	return g_strdup_printf ("<form><b>%s</b>&nbsp"
				"<select NAME=\"action\" SIZE=\"1\"> "
				"<option VALUE=\"S\">%s</option></select>&nbsp &nbsp "
				"<input TYPE=Submit name=\"ok\" value=\"%s\">"
				"</form>",
				_("Choose an action:"),
				_("Send Latest Information"),
				_("OK"));
}

static char*
get_cancel_options ()
{
	return g_strdup_printf ("<form><b>%s</b>&nbsp"
				"<select NAME=\"action\" SIZE=\"1\"> "
				"<option VALUE=\"C\">%s</option></select>&nbsp &nbsp "
				"<input TYPE=Submit name=\"ok\" value=\"%s\">"
				"</form>",
				_("Choose an action:"),
				_("Cancel"),
				_("OK"));
}


static CalComponent *
get_real_item (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	CalComponent *comp;
	CalComponentVType type;
	CalClientGetStatus status = CAL_CLIENT_GET_NOT_FOUND;
	const char *uid;
	
	priv = itip->priv;

	type = cal_component_get_vtype (priv->comp);
	cal_component_get_uid (priv->comp, &uid);

	switch (type) {
	case CAL_COMPONENT_EVENT:
		if (priv->event_client != NULL)
			status = cal_client_get_object (priv->event_client, uid, &comp);
		break;
	case CAL_COMPONENT_TODO:
		if (priv->task_client != NULL)
			status = cal_client_get_object (priv->task_client, uid, &comp);
		break;
	default:
		status = CAL_CLIENT_GET_NOT_FOUND;
	}

	if (status != CAL_CLIENT_GET_SUCCESS)
		return NULL;

	return comp;
}

static void
adjust_item (EItipControl *itip, CalComponent *comp)
{
	CalComponent *real_comp;
	
	real_comp = get_real_item (itip);
	if (real_comp != NULL) {
		CalComponentText text;
		const char *string;
		GSList *l;
		
		cal_component_get_summary (real_comp, &text);
		cal_component_set_summary (comp, &text);
		cal_component_get_location (real_comp, &string);
		cal_component_set_location (comp, string);
		cal_component_get_description_list (real_comp, &l);
		cal_component_set_description_list (comp, l);
		cal_component_free_text_list (l);
		
		g_object_unref (real_comp);
	} else {
		CalComponentText text = {_("Unknown"), NULL};
		
		cal_component_set_summary (comp, &text);
	}
}

static void
show_current_event (EItipControl *itip)
{
	EItipControlPrivate *priv;
	const gchar *itip_title, *itip_desc;
	char *options;

	priv = itip->priv;

	if (priv->calendar_uri)
		priv->event_client = start_calendar_server (itip, priv->calendar_uri);
	else 
		priv->event_client = find_server (priv->event_clients, priv->comp);
	
	switch (priv->method) {
	case ICAL_METHOD_PUBLISH:
		itip_desc = _("<b>%s</b> has published meeting information.");
		itip_title = _("Meeting Information");
		options = get_publish_options (priv->event_client ? FALSE : TRUE);
		break;
	case ICAL_METHOD_REQUEST:
		if (priv->delegator_address != NULL)
			itip_desc = _("<b>%s</b> requests the presence of %s at a meeting.");
		else
			itip_desc = _("<b>%s</b> requests your presence at a meeting.");
		itip_title = _("Meeting Proposal");
		options = get_request_options (priv->event_client ? FALSE : TRUE);
		break;
	case ICAL_METHOD_ADD:
		itip_desc = _("<b>%s</b> wishes to add to an existing meeting.");
		itip_title = _("Meeting Update");
		options = get_publish_options (priv->event_client ? FALSE : TRUE);
		break;
	case ICAL_METHOD_REFRESH:
		itip_desc = _("<b>%s</b> wishes to receive the latest meeting information.");
		itip_title = _("Meeting Update Request");
		options = get_refresh_options ();

		/* Provide extra info, since its not in the component */
		adjust_item (itip, priv->comp);
		break;
	case ICAL_METHOD_REPLY:
		itip_desc = _("<b>%s</b> has replied to a meeting request.");
		itip_title = _("Meeting Reply");
		options = get_reply_options ();

		/* Provide extra info, since might not be in the component */
		adjust_item (itip, priv->comp);
		break;
	case ICAL_METHOD_CANCEL:
		itip_desc = _("<b>%s</b> has cancelled a meeting.");
		itip_title = _("Meeting Cancellation");
		options = get_cancel_options ();

		/* Provide extra info, since might not be in the component */
		adjust_item (itip, priv->comp);
		break;
	default:
		itip_desc = _("<b>%s</b> has sent an unintelligible message.");
		itip_title = _("Bad Meeting Message");
		options = NULL;
	}

	write_html (itip, itip_desc, itip_title, options);
	g_free (options);
}

static void
show_current_todo (EItipControl *itip)
{
	EItipControlPrivate *priv;
	const gchar *itip_title, *itip_desc;
	char *options;

	priv = itip->priv;

	if (priv->calendar_uri)
		priv->task_client = start_calendar_server (itip, priv->calendar_uri);
	else 
		priv->task_client = find_server (priv->task_clients, priv->comp);

	switch (priv->method) {
	case ICAL_METHOD_PUBLISH:
		itip_desc = _("<b>%s</b> has published task information.");
		itip_title = _("Task Information");
		options = get_publish_options (priv->task_client ? FALSE : TRUE);
		break;
	case ICAL_METHOD_REQUEST:
		if (priv->delegator_address != NULL)
			itip_desc = _("<b>%s</b> requests %s to perform a task.");
		else
			itip_desc = _("<b>%s</b> requests you perform a task.");
		itip_title = _("Task Proposal");
		options = get_request_options (priv->task_client ? FALSE : TRUE);
		break;
	case ICAL_METHOD_ADD:
		itip_desc = _("<b>%s</b> wishes to add to an existing task.");
		itip_title = _("Task Update");
		options = get_publish_options (priv->task_client ? FALSE : TRUE);
		break;
	case ICAL_METHOD_REFRESH:
		itip_desc = _("<b>%s</b> wishes to receive the latest task information.");
		itip_title = _("Task Update Request");
		options = get_refresh_options ();

		/* Provide extra info, since its not in the component */
		adjust_item (itip, priv->comp);
		break;
	case ICAL_METHOD_REPLY:
		itip_desc = _("<b>%s</b> has replied to a task assignment.");
		itip_title = _("Task Reply");
		options = get_reply_options ();

		/* Provide extra info, since might not be in the component */
		adjust_item (itip, priv->comp);
		break;
	case ICAL_METHOD_CANCEL:
		itip_desc = _("<b>%s</b> has cancelled a task.");
		itip_title = _("Task Cancellation");
		options = get_cancel_options ();

		/* Provide extra info, since might not be in the component */
		adjust_item (itip, priv->comp);
		break;
	default:
		itip_desc = _("<b>%s</b> has sent an unintelligible message.");
		itip_title = _("Bad Task Message");
		options = NULL;
	}

	write_html (itip, itip_desc, itip_title, options);
	g_free (options);
}

static void
show_current_freebusy (EItipControl *itip)
{
	EItipControlPrivate *priv;
	const gchar *itip_title, *itip_desc;
	char *options;

	priv = itip->priv;

	switch (priv->method) {
	case ICAL_METHOD_PUBLISH:
		itip_desc = _("<b>%s</b> has published free/busy information.");
		itip_title = _("Free/Busy Information");
		options = NULL;
		break;
	case ICAL_METHOD_REQUEST:
		itip_desc = _("<b>%s</b> requests your free/busy information.");
		itip_title = _("Free/Busy Request");
		options = get_request_fb_options ();
		break;
	case ICAL_METHOD_REPLY:
		itip_desc = _("<b>%s</b> has replied to a free/busy request.");
		itip_title = _("Free/Busy Reply");
		options = NULL;
		break;
	default:
		itip_desc = _("<b>%s</b> has sent an unintelligible message.");
		itip_title = _("Bad Free/Busy Message");
		options = NULL;
	}

	write_html (itip, itip_desc, itip_title, options);
	g_free (options);
}

static icalcomponent *
get_next (icalcompiter *iter)
{
	icalcomponent *ret = NULL;
	icalcomponent_kind kind;

	do {
		icalcompiter_next (iter);
		ret = icalcompiter_deref (iter);
		kind = icalcomponent_isa (ret);
	} while (ret != NULL 
		 && kind != ICAL_VEVENT_COMPONENT
		 && kind != ICAL_VTODO_COMPONENT
		 && kind != ICAL_VFREEBUSY_COMPONENT);

	return ret;
}

static void
show_current (EItipControl *itip)
{
	EItipControlPrivate *priv;
	CalComponentVType type;
	icalcomponent *alarm_comp;
	icalcompiter alarm_iter;
	icalproperty *prop;

	priv = itip->priv;

	g_object_ref (itip);
	
	if (priv->comp)
		g_object_unref (priv->comp);
	if (priv->event_client != NULL)
		g_object_unref (priv->event_client);
	priv->event_client = NULL;
	if (priv->task_client != NULL)
		g_object_unref (priv->task_client);
	priv->task_client = NULL;

	/* Determine any delegate sections */
	prop = icalcomponent_get_first_property (priv->ical_comp, ICAL_X_PROPERTY);
	while (prop) {
		const char *x_name, *x_val;

		x_name = icalproperty_get_x_name (prop);
		x_val = icalproperty_get_x (prop);

		if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-CALENDAR-URI"))
			e_itip_control_set_calendar_uri (itip, x_val);
		else if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-ADDRESS"))
			e_itip_control_set_delegator_address (itip, x_val);
		else if (!strcmp (x_name, "X-EVOLUTION-DELEGATOR-NAME"))
			e_itip_control_set_delegator_name (itip, x_val);

		prop = icalcomponent_get_next_property (priv->ical_comp, ICAL_X_PROPERTY);
	}
	
	/* Strip out alarms for security purposes */
	alarm_iter = icalcomponent_begin_component (priv->ical_comp, ICAL_VALARM_COMPONENT);
	while ((alarm_comp = icalcompiter_deref (&alarm_iter)) != NULL) {
		icalcomponent_remove_component (priv->ical_comp, alarm_comp);
		
		icalcompiter_next (&alarm_iter);
	}

	priv->comp = cal_component_new ();
	if (!cal_component_set_icalcomponent (priv->comp, priv->ical_comp)) {
		write_error_html (itip, _("The message does not appear to be properly formed"));
		g_object_unref (priv->comp);
		priv->comp = NULL;
		g_object_unref (itip);
		return;
	};

	/* Add default reminder if the config says so */
	if (calendar_config_get_use_default_reminder ()) {
		CalComponentAlarm *acomp;
		int interval;
		CalUnits units;
		CalAlarmTrigger trigger;

		interval = calendar_config_get_default_reminder_interval ();
		units = calendar_config_get_default_reminder_units ();

		acomp = cal_component_alarm_new ();

		cal_component_alarm_set_action (acomp, CAL_ALARM_DISPLAY);

		trigger.type = CAL_ALARM_TRIGGER_RELATIVE_START;
		memset (&trigger.u.rel_duration, 0, sizeof (trigger.u.rel_duration));

		trigger.u.rel_duration.is_neg = TRUE;

		switch (units) {
		case CAL_MINUTES:
			trigger.u.rel_duration.minutes = interval;
			break;
		case CAL_HOURS:	
			trigger.u.rel_duration.hours = interval;
			break;
		case CAL_DAYS:	
			trigger.u.rel_duration.days = interval;
			break;
		default:
			g_assert_not_reached ();
		}

		cal_component_alarm_set_trigger (acomp, trigger);
		cal_component_add_alarm (priv->comp, acomp);

		cal_component_alarm_free (acomp);
	}

	type = cal_component_get_vtype (priv->comp);

	switch (type) {
	case CAL_COMPONENT_EVENT:
		if (!priv->event_clients)
			priv->event_clients = get_servers (itip, global_shell_client, calendar_types, FALSE);
		show_current_event (itip);
		break;
	case CAL_COMPONENT_TODO:
		if (!priv->task_clients)
			priv->task_clients = get_servers (itip, global_shell_client, tasks_types, FALSE);
		show_current_todo (itip);
		break;
	case CAL_COMPONENT_FREEBUSY:
		show_current_freebusy (itip);
		break;
	default:
		write_error_html (itip, _("The message contains only unsupported requests."));
	}

	find_my_address (itip, priv->ical_comp);

	g_object_unref (itip);
}

void
e_itip_control_set_data (EItipControl *itip, const gchar *text)
{
	EItipControlPrivate *priv;
	icalproperty *prop;
	icalcomponent_kind kind = ICAL_NO_COMPONENT;
	icalcomponent *tz_comp;
	icalcompiter tz_iter;

	priv = itip->priv;

	clean_up (itip);

	if (text == NULL || *text == '\0') {
		gtk_html_load_from_string (GTK_HTML (priv->html), " ", 1);
		return;
	}
	
	priv->vcalendar = g_strdup (text);
	priv->top_level = cal_util_new_top_level ();

	priv->main_comp = icalparser_parse_string (priv->vcalendar);
	if (priv->main_comp == NULL) {
		write_error_html (itip, _("The attachment does not contain a valid calendar message"));
		return;
	}

	prop = icalcomponent_get_first_property (priv->main_comp, ICAL_METHOD_PROPERTY);
	if (prop == NULL) {
		write_error_html (itip, _("The attachment does not contain a valid calendar message"));
		return;
	}

	priv->method = icalproperty_get_method (prop);

	tz_iter = icalcomponent_begin_component (priv->main_comp, ICAL_VTIMEZONE_COMPONENT);
	while ((tz_comp = icalcompiter_deref (&tz_iter)) != NULL) {
		icalcomponent *clone;

		clone = icalcomponent_new_clone (tz_comp);
		icalcomponent_add_component (priv->top_level, clone);

		icalcompiter_next (&tz_iter);
	}

	priv->iter = icalcomponent_begin_component (priv->main_comp, ICAL_ANY_COMPONENT);
	priv->ical_comp = icalcompiter_deref (&priv->iter);
	kind = icalcomponent_isa (priv->ical_comp);
	if (kind != ICAL_VEVENT_COMPONENT
	    && kind != ICAL_VTODO_COMPONENT
	    && kind != ICAL_VFREEBUSY_COMPONENT)
		priv->ical_comp = get_next (&priv->iter);

	if (priv->ical_comp == NULL) {
		write_error_html (itip, _("The attachment has no viewable calendar items"));		
		return;
	}

	priv->total = icalcomponent_count_components (priv->main_comp, ICAL_VEVENT_COMPONENT);
	priv->total += icalcomponent_count_components (priv->main_comp, ICAL_VTODO_COMPONENT);
	priv->total += icalcomponent_count_components (priv->main_comp, ICAL_VFREEBUSY_COMPONENT);

	if (priv->total > 0)
		priv->current = 1;
	else
		priv->current = 0;

	show_current (itip);
}

gchar *
e_itip_control_get_data (EItipControl *itip)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	return g_strdup (priv->vcalendar);
}

gint
e_itip_control_get_data_size (EItipControl *itip)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	if (priv->vcalendar == NULL)
		return 0;

	return strlen (priv->vcalendar);
}

void
e_itip_control_set_from_address (EItipControl *itip, const gchar *address)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	if (priv->from_address)
		g_free (priv->from_address);

	priv->from_address = g_strdup (address);
}

const gchar *
e_itip_control_get_from_address (EItipControl *itip)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	return priv->from_address;
}

void
e_itip_control_set_view_only (EItipControl *itip, gboolean view_only)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	priv->view_only = view_only;
}

gboolean
e_itip_control_get_view_only (EItipControl *itip)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	return priv->view_only;
}


void
e_itip_control_set_delegator_address (EItipControl *itip, const gchar *address)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	if (priv->delegator_address)
		g_free (priv->delegator_address);

	priv->delegator_address = g_strdup (address);
}

const gchar *
e_itip_control_get_delegator_address (EItipControl *itip)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	return priv->delegator_address;
}


void
e_itip_control_set_delegator_name (EItipControl *itip, const gchar *name)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	if (priv->delegator_name)
		g_free (priv->delegator_name);

	priv->delegator_name = g_strdup (name);
}

const gchar *
e_itip_control_get_delegator_name (EItipControl *itip)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	return priv->delegator_name;
}

void
e_itip_control_set_calendar_uri (EItipControl *itip, const gchar *uri)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	if (priv->calendar_uri)
		g_free (priv->calendar_uri);

	priv->calendar_uri = g_strdup (uri);
}

const gchar *
e_itip_control_get_calendar_uri (EItipControl *itip)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	return priv->calendar_uri;
}


static gboolean
change_status (icalcomponent *ical_comp, const char *address, icalparameter_partstat status)
{
	icalproperty *prop;

	prop = find_attendee (ical_comp, address);
	if (prop) {
		icalparameter *param;

		icalproperty_remove_parameter (prop, ICAL_PARTSTAT_PARAMETER);
		param = icalparameter_new_partstat (status);
		icalproperty_add_parameter (prop, param);
	} else {
		icalparameter *param;
		
		if (address != NULL) {
			prop = icalproperty_new_attendee (address);
			icalcomponent_add_property (ical_comp, prop);

			param = icalparameter_new_role (ICAL_ROLE_OPTPARTICIPANT);
			icalproperty_add_parameter (prop, param);

			param = icalparameter_new_partstat (status);
			icalproperty_add_parameter (prop, param);
		} else {
			EAccount *a;

			a = itip_addresses_get_default ();
			
			prop = icalproperty_new_attendee (a->id->address);
			icalcomponent_add_property (ical_comp, prop);
			
			param = icalparameter_new_cn (a->id->name);
			icalproperty_add_parameter (prop, param);	

			param = icalparameter_new_role (ICAL_ROLE_REQPARTICIPANT);
			icalproperty_add_parameter (prop, param);
			
			param = icalparameter_new_partstat (status);
			icalproperty_add_parameter (prop, param);
		}
	}

	return TRUE;
}

static void
update_item (EItipControl *itip)
{
	EItipControlPrivate *priv;
	struct icaltimetype stamp;
	icalproperty *prop;
	icalcomponent *clone;
	CalClient *client;
	CalComponentVType type;
	GtkWidget *dialog;
	CalClientResult result;

	priv = itip->priv;

	/* Set X-MICROSOFT-CDO-REPLYTIME to record the time at which
	 * the user accepted/declined the request. (Outlook ignores
	 * SEQUENCE in REPLY reponses and instead requires that each
	 * updated response have a later REPLYTIME than the previous
	 * one.) This also ends up getting saved in our own copy of
	 * the meeting, though there's currently no way to see that
	 * information (unless it's being saved to an Exchange folder
	 * and you then look at it in Outlook).
	 */
	stamp = icaltime_current_time_with_zone (icaltimezone_get_utc_timezone ());
	prop = icalproperty_new_x (icaltime_as_ical_string (stamp));
	icalproperty_set_x_name (prop, "X-MICROSOFT-CDO-REPLYTIME");
	icalcomponent_add_property (priv->ical_comp, prop);
 
	type = cal_component_get_vtype (priv->comp);
	if (type == CAL_COMPONENT_TODO)
		client = priv->task_client;
	else
		client = priv->event_client;

	clone = icalcomponent_new_clone (priv->ical_comp);
	icalcomponent_add_component (priv->top_level, clone);
	icalcomponent_set_method (priv->top_level, priv->method);

	result = cal_client_update_objects (client, priv->top_level);
	switch (result) {
	case CAL_CLIENT_RESULT_INVALID_OBJECT :
		dialog = gnome_warning_dialog (_("Object is invalid and cannot be updated\n"));
		break;
	case CAL_CLIENT_RESULT_CORBA_ERROR :
		dialog = gnome_warning_dialog (_("There was an error on the CORBA system\n"));
		break;
	case CAL_CLIENT_RESULT_NOT_FOUND :
		dialog = gnome_warning_dialog (_("Object could not be found\n"));
		break;
	case CAL_CLIENT_RESULT_PERMISSION_DENIED :
		dialog = gnome_warning_dialog (_("You do not have the right permissions to update the calendar\n"));
		break;
	case CAL_CLIENT_RESULT_SUCCESS :
		dialog = gnome_ok_dialog (_("Update complete\n"));
		break;
	default :
		dialog = gnome_warning_dialog (_("Calendar file could not be updated!\n"));
		break;
	}
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));

	icalcomponent_remove_component (priv->top_level, clone);
}

static void
update_attendee_status (EItipControl *itip)
{
	EItipControlPrivate *priv;
	CalClient *client;
	CalClientGetStatus status;
	CalComponent *comp = NULL;
	CalComponentVType type;
	const char *uid;
	GtkWidget *dialog;
	CalClientResult result;

	priv = itip->priv;

	type = cal_component_get_vtype (priv->comp);
	if (type == CAL_COMPONENT_TODO)
		client = priv->task_client;
	else
		client = priv->event_client;

	if (client == NULL) {
		dialog = gnome_warning_dialog (_("Attendee status can not be updated "
						 "because the item no longer exists"));
		goto cleanup;
	}
	
	/* Obtain our version */
	cal_component_get_uid (priv->comp, &uid);
	status = cal_client_get_object (client, uid, &comp);

	if (status == CAL_CLIENT_GET_SUCCESS) {
		GSList *attendees;

		cal_component_get_attendee_list (priv->comp, &attendees);
		if (attendees != NULL) {
			CalComponentAttendee *a = attendees->data;
			icalproperty *prop;

			prop = find_attendee (cal_component_get_icalcomponent (comp),
					      itip_strip_mailto (a->value));
			
			if (prop == NULL) {
				dialog = gnome_question_dialog_modal (_("This response is not from a current "
									"attendee.  Add as an attendee?"),
								      NULL, NULL);
				if (gnome_dialog_run_and_close (GNOME_DIALOG (dialog)) == GNOME_YES) {
					change_status (cal_component_get_icalcomponent (comp),
						       itip_strip_mailto (a->value),
						       a->status);
					cal_component_rescan (comp);
				} else {
					goto cleanup;
				}
			} else if (a->status == ICAL_PARTSTAT_NONE || a->status == ICAL_PARTSTAT_X) {
				dialog = gnome_warning_dialog (_("Attendee status could "
								 "not be updated because "
								 "of an invalid status!\n"));
				goto cleanup;
			} else {
				change_status (cal_component_get_icalcomponent (comp),
					       itip_strip_mailto (a->value),
					       a->status);
				cal_component_rescan (comp);				
			}
		}

		result = cal_client_update_object (client, comp);
		switch (result) {
		case CAL_CLIENT_RESULT_INVALID_OBJECT :
			dialog = gnome_warning_dialog (_("Object is invalid and cannot be updated\n"));
			break;
		case CAL_CLIENT_RESULT_CORBA_ERROR :
			dialog = gnome_warning_dialog (_("There was an error on the CORBA system\n"));
			break;
		case CAL_CLIENT_RESULT_NOT_FOUND :
			dialog = gnome_warning_dialog (_("Object could not be found\n"));
			break;
		case CAL_CLIENT_RESULT_PERMISSION_DENIED :
			dialog = gnome_warning_dialog (_("You don't have the right permissions to update the calendar\n"));
			break;
		case CAL_CLIENT_RESULT_SUCCESS :
			dialog = gnome_ok_dialog (_("Attendee status updated\n"));
			break;
		default :
			dialog = gnome_warning_dialog (_("Attendee status could not be updated!\n"));
		}
	} else {
		dialog = gnome_warning_dialog (_("Attendee status can not be updated "
						 "because the item no longer exists"));
	}

 cleanup:
	if (comp != NULL)
		g_object_unref (comp);
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
remove_item (EItipControl *itip)
{
	EItipControlPrivate *priv;
	CalClient *client;
	CalComponentVType type;
	const char *uid;
	GtkWidget *dialog;

	priv = itip->priv;

	type = cal_component_get_vtype (priv->comp);
	if (type == CAL_COMPONENT_TODO)
		client = priv->task_client;
	else
		client = priv->event_client;

	if (client == NULL)
		return;
	
	cal_component_get_uid (priv->comp, &uid);
	if (cal_client_remove_object (client, uid) == CAL_CLIENT_RESULT_SUCCESS) {
		dialog = gnome_ok_dialog (_("Removal Complete"));
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	}
}

static void
send_item (EItipControl *itip)
{
	EItipControlPrivate *priv;
	CalComponent *comp;
	CalComponentVType vtype;
	GtkWidget *dialog;

	priv = itip->priv;

	comp = get_real_item (itip);
	vtype = cal_component_get_vtype (comp);
	
	if (comp != NULL) {
		switch (vtype) {
		case CAL_COMPONENT_EVENT:
			itip_send_comp (CAL_COMPONENT_METHOD_REQUEST, comp, priv->event_client, NULL);
			break;
		case CAL_COMPONENT_TODO:
			itip_send_comp (CAL_COMPONENT_METHOD_REQUEST, comp, priv->task_client, NULL);
			break;
		default:
			itip_send_comp (CAL_COMPONENT_METHOD_REQUEST, comp, NULL, NULL);
		}
		g_object_unref (comp);
		dialog = gnome_ok_dialog (_("Item sent!\n"));
	} else {
		dialog = gnome_warning_dialog (_("The item could not be sent!\n"));
	}
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
send_freebusy (EItipControl *itip)
{
	EItipControlPrivate *priv;
	CalComponentDateTime datetime;
	time_t start, end;
	GtkWidget *dialog;
	GList *comp_list;
	icaltimezone *zone;

	priv = itip->priv;

	cal_component_get_dtstart (priv->comp, &datetime);
	if (datetime.tzid) {
		zone = icalcomponent_get_timezone (priv->top_level,
						   datetime.tzid);
	} else {
		zone = NULL;
	}
	start = icaltime_as_timet_with_zone (*datetime.value, zone);
	cal_component_free_datetime (&datetime);

	cal_component_get_dtend (priv->comp, &datetime);
	if (datetime.tzid) {
		zone = icalcomponent_get_timezone (priv->top_level,
						   datetime.tzid);
	} else {
		zone = NULL;
	}
	end = icaltime_as_timet_with_zone (*datetime.value, zone);
	cal_component_free_datetime (&datetime);

	comp_list = cal_client_get_free_busy (priv->event_client, NULL, start, end);

	if (comp_list) {
		GList *l;

		for (l = comp_list; l; l = l->next) {
			CalComponent *comp = CAL_COMPONENT (l->data);
			itip_send_comp (CAL_COMPONENT_METHOD_REPLY, comp, priv->event_client, NULL);

			g_object_unref (comp);
		}
		dialog = gnome_ok_dialog (_("Item sent!\n"));

		g_list_free (comp_list);
	} else {
		dialog = gnome_warning_dialog (_("The item could not be sent!\n"));
	}
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
button_selected_cb (EvolutionFolderSelectorButton *button, GNOME_Evolution_Folder *folder, gpointer data)
{
	EItipControl *itip = E_ITIP_CONTROL (data);
	EItipControlPrivate *priv;
	CalComponentVType type;
	char *uri;
	
	priv = itip->priv;
	
	type = cal_component_get_vtype (priv->comp);
	if (type == CAL_COMPONENT_TODO)
		uri = cal_util_expand_uri (folder->physicalUri, TRUE);
	else
		uri = cal_util_expand_uri (folder->physicalUri, FALSE);

	g_object_unref (priv->event_client);
	priv->event_client = start_calendar_server (itip, uri);

	g_free (uri);
}

static void
url_requested_cb (GtkHTML *html, const gchar *url, GtkHTMLStream *handle, gpointer data)
{	unsigned char buffer[4096];
	int len, fd;
        char *path;

	path = g_strdup_printf ("%s/%s", EVOLUTION_IMAGESDIR, url);

	if ((fd = open (path, O_RDONLY)) == -1) {
		g_warning ("%s", g_strerror (errno));
		goto cleanup;
	}

       	while ((len = read (fd, buffer, 4096)) > 0) {
		gtk_html_write (html, handle, buffer, len);
	}

	if (len < 0) {
		/* check to see if we stopped because of an error */
		gtk_html_end (html, handle, GTK_HTML_STREAM_ERROR);
		g_warning ("%s", g_strerror (errno));
		goto cleanup;
	}
	/* done with no errors */
	gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
	close (fd);

 cleanup:
	g_free (path);
}

static gboolean
object_requested_cb (GtkHTML *html, GtkHTMLEmbedded *eb, gpointer data) 
{
	EItipControl *itip = E_ITIP_CONTROL (data);
	EItipControlPrivate *priv;
	GtkWidget *button;
	CalComponentVType vtype;
	
	priv = itip->priv;	

	vtype = cal_component_get_vtype (priv->comp);

	switch (vtype) {
	case CAL_COMPONENT_EVENT:
		button = evolution_folder_selector_button_new (
			global_shell_client, _("Select Calendar Folder"),
			calendar_config_default_calendar_folder (), 
			calendar_types);
		priv->event_client = start_default_server (itip, FALSE);
		break;
	case CAL_COMPONENT_TODO:
		button = evolution_folder_selector_button_new (
			global_shell_client, _("Select Tasks Folder"),
			calendar_config_default_tasks_folder (), 
			tasks_types);
		priv->task_client = start_default_server (itip, TRUE);
		break;
	default:
		button = NULL;
	}

	g_signal_connect (button, "selected", G_CALLBACK (button_selected_cb), itip);
	
	gtk_container_add (GTK_CONTAINER (eb), button);
	gtk_widget_show (button);

	return TRUE;
}

static void
ok_clicked_cb (GtkHTML *html, const gchar *method, const gchar *url, const gchar *encoding, gpointer data)
{
	EItipControl *itip = E_ITIP_CONTROL (data);
	EItipControlPrivate *priv;
	gchar **fields;
	gboolean rsvp = FALSE, status = FALSE;
	int i;

	priv = itip->priv;

	fields = g_strsplit (encoding, "&", -1);
	for (i = 0; fields[i] != NULL; i++) {
		gchar **key_value;

		key_value = g_strsplit (fields[i], "=", 2);

		if (key_value[0] != NULL && !strcmp (key_value[0], "action")) {
			if (key_value[1] == NULL)
				break;

			switch (key_value[1][0]) {
			case 'U':
				update_item (itip);
				break;
			case 'A':
				status = change_status (priv->ical_comp, priv->my_address, 
							ICAL_PARTSTAT_ACCEPTED);
				if (status) {
					cal_component_rescan (priv->comp);
					update_item (itip);
				}
				break;
			case 'T':
				status = change_status (priv->ical_comp, priv->my_address,
							ICAL_PARTSTAT_TENTATIVE);
				if (status) {
					cal_component_rescan (priv->comp);
					update_item (itip);
				}
				break;
			case 'D':
				status = change_status (priv->ical_comp, priv->my_address,
							ICAL_PARTSTAT_DECLINED);
				if (status) {
					cal_component_rescan (priv->comp);
					remove_item (itip);
				}
				break;
			case 'F':
				send_freebusy (itip);
				break;
			case 'R':
				update_attendee_status (itip);
				break;
			case 'S':
				send_item (itip);
				break;
			case 'C':
				update_item (itip);
				break;
			}
		}

		if (key_value[0] != NULL && !strcmp (key_value[0], "rsvp"))
			if (*key_value[1] == '1')
				rsvp = TRUE;

		g_strfreev (key_value);

	}
	g_strfreev (fields);

	if (rsvp && status) {
		CalComponent *comp = NULL;
		CalComponentVType vtype;
		icalcomponent *ical_comp;
		icalproperty *prop;
		icalvalue *value;
		const char *attendee;
		GSList *l, *list = NULL;
		
		comp = cal_component_clone (priv->comp);
		if (comp == NULL)
			return;
		vtype = cal_component_get_vtype (comp);
		
		if (priv->my_address == NULL)
			find_my_address (itip, priv->ical_comp);
		g_assert (priv->my_address != NULL);
		
		ical_comp = cal_component_get_icalcomponent (comp);
		
		for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
		     prop != NULL;
		     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY))
		{
			char *text;
			
			value = icalproperty_get_value (prop);
			if (!value)
				continue;
			
			attendee = icalvalue_get_string (value);
			
			text = g_strdup (itip_strip_mailto (attendee));
			text = g_strstrip (text);
			if (g_strcasecmp (priv->my_address, text))
				list = g_slist_prepend (list, prop);
			g_free (text);
		}
		
		for (l = list; l; l = l->next) {
			prop = l->data;
			icalcomponent_remove_property (ical_comp, prop);
			icalproperty_free (prop);
		}
		g_slist_free (list);
		
		cal_component_rescan (comp);
		switch (vtype) {
		case CAL_COMPONENT_EVENT:
			itip_send_comp (CAL_COMPONENT_METHOD_REPLY, comp,
					priv->event_client, priv->top_level);
			break;
		case CAL_COMPONENT_TODO:
			itip_send_comp (CAL_COMPONENT_METHOD_REPLY, comp,
					priv->task_client, priv->top_level);
			break;
		default:
			itip_send_comp (CAL_COMPONENT_METHOD_REPLY, comp, NULL, NULL);
		}
		g_object_unref (comp);
	}
}
