/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary.h
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

#ifndef _E_SUMMARY_H__
#define _E_SUMMARY_H__

#include <gtk/gtkvbox.h>

#include <bonobo.h>
#include <bonobo/bonobo-listener.h>
#include <bonobo/bonobo-event-source.h>
#include <Evolution.h>
#include <evolution-services/Executive-Summary.h>

#include "e-summary-prefs.h"

#define E_SUMMARY_TYPE (e_summary_get_type ())
#define E_SUMMARY(obj) (GTK_CHECK_CAST ((obj), E_SUMMARY_TYPE, ESummary))
#define E_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_SUMMARY_TYPE, ESummaryClass))
#define IS_E_SUMMARY(obj) (GTK_CHECK_TYPE ((obj), E_SUMMARY_TYPE))
#define IS_E_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_SUMMARY_TYPE))

typedef struct _ESummaryPrivate ESummaryPrivate;
typedef struct _ESummary ESummary;
typedef struct _ESummaryClass ESummaryClass;
typedef struct _ESummaryWindow ESummaryWindow;

struct _ESummaryWindow {
	GNOME_Evolution_Summary_Component component;

	Bonobo_Control control;
	GNOME_Evolution_Summary_HTMLView html;

	Bonobo_PersistStream persiststream;
	Bonobo_PropertyBag propertybag;
	Bonobo_PropertyControl propertycontrol;
	Bonobo_EventSource event_source;

	BonoboPropertyListener *listener;
	BonoboListener *html_listener;
	Bonobo_Listener html_corba_listener;

	char *iid;
	char *title;
	char *icon;
	
	ESummary *esummary;
};

struct _ESummary {
	GtkVBox parent;

	ESummaryPrefs *prefs;
	ESummaryPrefs *tmp_prefs;
	ESummaryPrivate *private;
};

struct _ESummaryClass {
	GtkVBoxClass parent_class;
};

GtkType e_summary_get_type (void);
GtkWidget *e_summary_new (const GNOME_Evolution_Shell shell);
void e_summary_queue_rebuild (ESummary *esummary);

void e_summary_window_free (ESummaryWindow *window);
void e_summary_remove_window (ESummary *esummary,
			      ESummaryWindow *window);
ESummaryWindow *e_summary_add_service (ESummary *esummary,
				       GNOME_Evolution_Summary_Component component,
				       const char *iid);
ESummaryWindow * e_summary_embed_service_from_id (ESummary *esummary,
						  const char *obj_id);

void e_summary_set_shell_view_interface (ESummary *summary,
					 GNOME_Evolution_ShellView svi);
void e_summary_set_message (ESummary *esummary,
			    const char *message,
			    gboolean busy);
void e_summary_unset_message (ESummary *esummary);
void e_summary_change_current_view (ESummary *esummary,
				    const char *uri);
void e_summary_set_title (ESummary *esummary,
			  const char *title);

void e_summary_window_move_left (ESummary *esummary,
				 ESummaryWindow *window);
void e_summary_window_move_right (ESummary *esummary,
				  ESummaryWindow *window);
void e_summary_window_move_up (ESummary *esummary,
			       ESummaryWindow *window);
void e_summary_window_move_down (ESummary *esummary,
				 ESummaryWindow *window);
void e_summary_reconfigure (ESummary *esummary);

#endif
