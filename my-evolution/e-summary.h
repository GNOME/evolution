/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary.h
 *
 * Copyright (C) 2001 Ximian, Inc.
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
 * Author: Iain Holmes
 */

#ifndef _E_SUMMARY_H__
#define _E_SUMMARY_H__

#include <gtk/gtkvbox.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-control.h>
#include "e-summary-type.h"
#include "e-summary-mail.h"
#include "e-summary-calendar.h"
#include "e-summary-rdf.h"
#include "e-summary-weather.h"
#include "e-summary-tasks.h"

#include <ical.h>

#include <Evolution.h>

#define E_SUMMARY_TYPE (e_summary_get_type ())
#define E_SUMMARY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_SUMMARY_TYPE, ESummary))
#define E_SUMMARY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), E_SUMMARY_TYPE, ESummaryClass))
#define IS_E_SUMMARY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_SUMMARY_TYPE))
#define IS_E_SUMMARY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), E_SUMMARY_TYPE))

typedef struct _ESummaryPrivate ESummaryPrivate;
typedef struct _ESummaryClass ESummaryClass;
typedef struct _ESummaryPrefsFolder ESummaryPrefsFolder;
typedef struct _ESummaryPrefs ESummaryPrefs;
typedef struct _ESummaryConnection ESummaryConnection;
typedef struct _ESummaryConnectionData ESummaryConnectionData;

typedef void (* ESummaryProtocolListener) (ESummary *summary,
					   const char *uri,
					   void *closure);
typedef int (* ESummaryConnectionCount) (ESummary *summary,
					 void *closure);
typedef GList *(* ESummaryConnectionAdd) (ESummary *summary,
					  void *closure);
typedef void (* ESummaryConnectionSetOnline) (ESummary *summary,
					      GNOME_Evolution_OfflineProgressListener progress,
					      gboolean online,
					      void *closure);
typedef void (*ESummaryOnlineCallback) (ESummary *summary,
					void *closure);

struct _ESummaryConnection {
	ESummaryConnectionCount count;
	ESummaryConnectionAdd add;
	ESummaryConnectionSetOnline set_online;
	ESummaryOnlineCallback callback;

	void *closure;
	void *callback_closure;
};

struct _ESummaryConnectionData {
	char *hostname;
	char *type;
};

struct _ESummaryPrefsFolder {
	char *physical_uri;
	char *evolution_uri;
};

struct _ESummaryPrefs {

	/* Mail */
	GSList *display_folders; /* List of ESummaryPrefsFolder */
	gboolean show_full_path;

	/* RDF */
	GSList *rdf_urls;
	int rdf_refresh_time;
	int limit;

	/* Weather */
	GSList *stations;
	ESummaryWeatherUnits units;
	int weather_refresh_time;

	/* Schedule */
	ESummaryCalendarDays days;
	ESummaryCalendarNumTasks show_tasks;
};

struct _ESummary {
	GtkVBox parent;

	ESummaryPrefs *preferences;

	ESummaryMail *mail;
	ESummaryCalendar *calendar;
	ESummaryRDF *rdf;
	ESummaryWeather *weather;
	ESummaryTasks *tasks;

	ESummaryPrivate *priv;

	gboolean online;

	char *timezone;
	icaltimezone *tz;
};

struct _ESummaryClass {
	GtkVBoxClass parent_class;
};


GtkType e_summary_get_type (void);
GtkWidget *e_summary_new (ESummaryPrefs *prefs);

BonoboControl *e_summary_get_control (ESummary *summary);
void e_summary_set_control (ESummary *summary, 
			    BonoboControl *control);

void e_summary_print (BonoboUIComponent *component,
		      gpointer user_data,
		      const char *cname);
void e_summary_reload (BonoboUIComponent *component,
		       gpointer user_data,
		       const char *cname);
void e_summary_draw (ESummary *summary);
void e_summary_redraw_all (void);

void e_summary_change_current_view (ESummary *summary,
				    const char *uri);

void e_summary_set_message (ESummary *summary,
			    const char *message,
			    gboolean busy);
void e_summary_unset_message (ESummary *summary);

void e_summary_add_protocol_listener (ESummary *summary,
				      const char *protocol,
				      ESummaryProtocolListener listener,
				      void *closure);

void e_summary_reconfigure (ESummary *summary);
void e_summary_reconfigure_all (void);

int e_summary_count_connections (ESummary *summary);
GList *e_summary_add_connections (ESummary *summary);
void e_summary_set_online (ESummary *summary,
			   GNOME_Evolution_OfflineProgressListener listener,
			   gboolean online,
			   ESummaryOnlineCallback callback,
			   void *closure);
void e_summary_add_online_connection (ESummary *summary,
				      ESummaryConnection *connection);
void e_summary_remove_online_connection  (ESummary *summary,
					  ESummaryConnection *connection);

void e_summary_freeze (ESummary *summary);
void e_summary_thaw (ESummary *summary);
#endif
